# 1.1 — parallel_for

`parallel_for` is the workhorse of oneTBB: you declare an iteration space, assert that
iterations are **independent**, and the scheduler splits the space into subranges, turns
each subrange into a **task**, and load-balances them across workers (Part 0.3). Every
other parallel algorithm in this part is a variation on the same split → execute → join
pattern; start here.

```
   blocked_range[begin, end)
        └─ split ─┬─ subrange A ──▶ task ──▶ worker 0 (LIFO from local deque)
                  └─ subrange B ──▶ task ──▶ worker 1 (stolen FIFO if idle)
```

---

## 1.1.1 Two forms: index range vs blocked_range

oneTBB exposes two entry points. Both end up as range splits and tasks; pick the one that
matches how you think about the loop.

**Form A — index range** (convenience for 1-D loops with step 1 or a custom step):

```cpp
tbb::parallel_for(first, last, body);           // step = 1
tbb::parallel_for(first, last, step, body);     // e.g. stride-2
```

The body receives a single index `i`, not a subrange. The runtime still chunks indices
internally.

**Form B — `blocked_range` + subrange body** (the general form; required for 2-D/3-D and
custom ranges):

```cpp
tbb::parallel_for(
    tbb::blocked_range<size_t>(begin, end, grainsize),
    [&](const tbb::blocked_range<size_t>& r) {
        for (size_t i = r.begin(); i != r.end(); ++i)
            work(i);
    });
```

> **The API ▸**
> ```cpp
> // <oneapi/tbb/parallel_for.h>
> template<typename Index, typename Function>
> void parallel_for(Index first, Index last, const Function& f);
> template<typename Index, typename Function>
> void parallel_for(Index first, Index last, Index step, const Function& f);
> template<typename Range, typename Body>
> void parallel_for(const Range& range, const Body& body,
>                   const auto_partitioner& = auto_partitioner{});
> template<typename Range, typename Body, typename Partitioner>
> void parallel_for(const Range& range, const Body& body, Partitioner partitioner);
> ```
> **Semantics:** `parallel_for` does not return until every subrange has been processed.
> Iterations must be **independent** — no iteration may read/write state that another
> iteration also mutates unless you synchronize (which defeats the purpose).

![Range splitting for parallel_for: recursive bisection until grainsize](figures/parallel-for.svg)

---

## 1.1.2 Independence requirement

The scheduler assumes any subrange can run before, after, or concurrently with any other.
Violating that assumption produces data races — undefined behavior, not a polite TBB error.

```
   ✓  output[i] = f(input[i])           each i touches distinct slots
   ✓  matrix[i][j] += 1                 2-D blocked_range, distinct (i,j)
   ✗  shared += data[i]                 use parallel_reduce instead
   ✗  output[i] = input[i-1]            loop-carried dependency → serial or scan
   ✗  v.push_back(compute(i))           use concurrent_vector (Part 3.1)
```

If iterations differ wildly in cost, independence still holds but **load balance** suffers
unless you tune grain size (Part 0.4) or choose a partitioner (Part 0.5).

---

## 1.1.3 How the range becomes tasks

Mechanically, `parallel_for` wraps your body in a **range task** that:

1. Takes the current range (initially the full `[begin, end)`).
2. Asks the **partitioner** whether to split (Part 0.5).
3. If yes → bisects the range, spawns the right half as a new task, recurses on the left
   (classic divide-and-conquer on the index space).
4. If no (subrange ≤ grainsize) → runs your body on that subrange **serially**.

```
   [0 ─────────────── 1M)
         split
    [0 ── 500K)              [500K ── 1M)
       split                      ...
   [0─125K) [125K─250K)     ... tasks until ≤ grainsize
       │         │
       ▼         ▼
    body(r)   body(r)     ← each body runs a plain for-loop over r
```

> **Under the hood ▸** Splitting is cheap (pointer arithmetic on the range object). Task
> spawn is ~tens of nanoseconds. The expensive part is **too many** tiny tasks: each split
> adds scheduler overhead. That is why grainsize exists — it sets the smallest chunk the
> partitioner will treat as indivisible (Part 0.4).

---

## 1.1.4 Grainsize and partitioners

Pass grainsize as the third argument to `blocked_range`:

```cpp
// At most ~N/grainsize leaf tasks; each leaf does ≥ grainsize iterations.
tbb::blocked_range<size_t>(0, n, /*grainsize=*/10'000)
```

| Knob | Where | Effect |
|------|-------|--------|
| **grainsize** | `blocked_range(begin, end, grainsize)` | Floor on subrange size; stops split tree |
| **partitioner** | 3rd arg to `parallel_for` | *How* and *whether* to split further |

Default is `auto_partitioner()` — adaptive splitting with work-stealing. Alternatives:

- `simple_partitioner()` — always split to grainsize; deterministic split tree.
- `static_partitioner()` — pre-cut at grainsize; minimal split overhead, poor on skewed work.
- `affinity_partitioner()` — reuse split decisions across repeated calls (Part 0.5).

**Tuning ▸** Target **10–100 µs of work per leaf task**. Measure with your real body on
target hardware; grainsize is not portable in iteration counts.

---

## 1.1.5 Lambda capture correctness

The body may run on any worker, any number of times, in any order. Capture rules:

```cpp
std::vector<double> a(n);

// ✓ Capture by reference — a is shared read/write at distinct indices (if independent)
tbb::parallel_for(0, n, [&](size_t i) { a[i] *= 2.0; });

// ✗ Capture loop index i from an outer serial loop — classic stale-closure bug
for (int round = 0; round < 3; ++round)
    tbb::parallel_for(0, n, [&](size_t i) { a[i] += round; });  // OK: round is fixed per call

// ✗ [&] on a variable mutated by another iteration
int counter = 0;
tbb::parallel_for(0, n, [&](size_t i) { ++counter; });  // DATA RACE

// ✓ Copy what each task needs by value into the lambda's closure
double factor = compute_factor();
tbb::parallel_for(0, n, [&, factor](size_t i) { a[i] *= factor; });
```

> **Pitfall ▸** Ignoring the subrange `r` and looping `[0, n)` inside the body executes
> every iteration on **every** task — wrong results and catastrophic overhead. Always
> iterate `[r.begin(), r.end())`.

---

## 1.1.6 Nesting

Nested `parallel_for` is idiomatic and **safe** on oneTBB scheduler: inner and outer loops
become tasks on the **same** worker pool (~one thread per core). You do not get the
`std::thread`-inside-`std::thread` explosion (Part 0.1).

```
   outer parallel_for
        └─ task on worker W
              └─ inner parallel_for  →  more tasks on same pool
                    (W may execute them, or stealers may help)
```

**Trade-offs ▸** Deep nesting with tiny inner ranges creates many small tasks. Prefer one
multi-dimensional `blocked_range2d` when the inner loop is regular, or increase inner
grainsize so the outer level keeps most of the parallelism.

---

## 1.1.7 Full example: scale and clamp a buffer

```cpp
// g++ -std=c++17 -O2 parallel_for_demo.cpp -ltbb -o parallel_for_demo
#include <oneapi/tbb/parallel_for.h>
#include <oneapi/tbb/blocked_range.h>
#include <oneapi/tbb/partitioner.h>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <vector>

int main() {
    const size_t n = 10'000'000;
    std::vector<double> a(n);

    // Form A: index-range syntax
    tbb::parallel_for(size_t{0}, n, [&](size_t i) {
        a[i] = std::sin(i * 0.001);
    });

    // Form B: blocked_range + grainsize + auto_partitioner
    tbb::parallel_for(
        tbb::blocked_range<size_t>(0, n, 8192),
        [&](const tbb::blocked_range<size_t>& r) {
            for (size_t i = r.begin(); i != r.end(); ++i)
                a[i] = std::clamp(a[i] * 2.0 + 1.0, -1.0, 1.0);
        },
        tbb::auto_partitioner{});

    std::printf("a[0]=%g  a[%zu]=%g\n", a.front(), n - 1, a.back());
    return 0;
}
```

---

## Summary

- `parallel_for` has **two forms**: index range (`first`, `last`, `[step]`, `body`) and
  general **`blocked_range` + subrange body**.
- Iterations must be **independent**; reductions, dependencies, and shared mutation need
  other algorithms or containers.
- The runtime **recursively splits** the range into tasks until subranges reach **grainsize**;
  partitioners control splitting policy (Part 0.4, Part 0.5).
- Capture lambdas carefully; loop only over **`r`**, not the full range.
- **Nested** `parallel_for` composes on one scheduler without oversubscribing cores.

Next: [1.2 — parallel_reduce](02-parallel-reduce.md)
