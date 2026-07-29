# 5.3 — Thread-Local Storage (enumerable_thread_specific)

Parts 5.1–5.2 fixed **allocator** and **layout** bottlenecks. The third pattern
for eliminating write contention is simpler in intent: give **each worker its own
copy** of mutable state, accumulate locally without locks, then merge once.
`enumerable_thread_specific<T>` (**ETS**) is TBB's typed, enumerable wrapper
around that idea — the mechanism behind efficient reductions and the standard
fix for shared-write hot spots.

---

## 5.3.1 The shared-write problem

A single counter updated by every task:

```
   task₁ ──fetch_add──▶ [ shared sum ] ◀──fetch_add── task₂
   task₃ ──fetch_add──▶     ▲          ◀──fetch_add── task₄
                            │
                     cache-line ping-pong
                     (Part 5.2) + atomic RMW cost
```

`parallel_reduce` solves this by giving each body a **local accumulator** and
merging at join time. But what if you need per-thread state that **persists
across multiple parallel regions**, or a histogram, or a scratch buffer?
`thread_local` is not enumerable — you cannot iterate all copies to merge.
**ETS** fills that gap.

![Each worker owns a lock-free local copy; combine() folds them after parallel work](figures/ets.svg)

```
   enumerable_thread_specific<int> sums(0);

   worker 0 → local copy: 42
   worker 1 → local copy: 37     combine() → 42 + 37 + … = total
   worker 2 → local copy: 51
```

> **Under the hood ▸** ETS lazily constructs one `T` per participating worker
> thread (keyed by thread identity). The default constructor uses
> `ets_no_key` — instances are looked up by thread id. Access via `local()` is
> lock-free after first touch. Copies are cache-padded internally to reduce false
> sharing between the ETS metadata and adjacent objects.

---

## 5.3.2 The API in practice

> **The API ▸**
>
> ```cpp
> #include <oneapi/tbb/enumerable_thread_specific.h>
>
> tbb::enumerable_thread_specific<int> sums(0);           // default init
> tbb::enumerable_thread_specific<std::vector<int>> bufs([]{ return std::vector<int>{}; });
>
> int& mine = sums.local();              // this thread's copy (created on first use)
> int total = sums.combine([](int a, int b){ return a + b; });
> sums.combine_each([](int& x){ x *= 2; });  // mutate each copy in place
> ```
>
> `ets_key_usage_type`: `ets_no_key` (default) vs `ets_key_per_instance` for
> finer-grained keys when one ETS object tracks multiple logical slots per thread.

```cpp
// build: g++ -std=c++17 -O2 ets_sum.cpp -ltbb -o ets_sum
#include <oneapi/tbb/enumerable_thread_specific.h>
#include <oneapi/tbb/parallel_for.h>
#include <oneapi/tbb/blocked_range.h>
#include <cstdio>
#include <vector>

int main() {
    const int n = 10'000'000;
    std::vector<int> data(n, 1);

    tbb::enumerable_thread_specific<long long> local_sum(0);

    tbb::parallel_for(tbb::blocked_range<int>(0, n),
        [&](const tbb::blocked_range<int>& r) {
            long long& mine = local_sum.local();
            for (int i = r.begin(); i != r.end(); ++i)
                mine += data[i];
        });

    long long total = local_sum.combine(
        [](long long a, long long b) { return a + b; });

    std::printf("sum=%lld  thread-local copies=%zu\n",
                total, std::distance(local_sum.begin(), local_sum.end()));
    return 0;
}
```

Each worker accumulates into its own `long long` — no atomics, no false sharing
on the accumulator itself. The final `combine` is serial but cheap (one pass over
~P copies, not N elements).

---

## 5.3.3 combine, combine_each, and iteration

Three ways to read out thread-local state:

| Method | Use when |
|--------|----------|
| range-for `for (auto& x : ets)` | inspect or manually fold each copy |
| `combine(f)` | associative fold returning one value (sum, min, merge) |
| `combine_each(f)` | in-place mutation of every copy |

Histogram example — each thread owns a bin vector, merged once:

```cpp
const int bins = 16;
tbb::enumerable_thread_specific<std::vector<int>> local_hist(
    [] { return std::vector<int>(bins, 0); });

tbb::parallel_for(tbb::blocked_range<size_t>(0, data.size()),
    [&](const tbb::blocked_range<size_t>& r) {
        auto& h = local_hist.local();
        for (size_t i = r.begin(); i != r.end(); ++i)
            ++h[data[i] % bins];
    });

std::vector<int> global(bins, 0);
for (const auto& h : local_hist)
    for (int b = 0; b < bins; ++b)
        global[b] += h[b];
```

For a custom merge type, give `T` a `merge` member or use `combine` with a
lambda that knows your algebra.

---

## 5.3.4 ETS vs combinable vs parallel_reduce

| Construct | When to use |
|-----------|-------------|
| `parallel_reduce` | one-shot fold over a range; body gets local seed automatically |
| `combinable<T>` | like ETS but **lazy** `local()` without default constructor; lighter for simple folds |
| `enumerable_thread_specific<T>` | arbitrary `T`, custom init lambda, iterate/combine copies, reuse across calls |

> **The API ▸** `combinable<T>` — `#include <oneapi/tbb/combinable.h>`
>
> ```cpp
> tbb::combinable<int> c;
> c.local() += 1;
> int total = c.combine([](int a, int b){ return a + b; });
> ```
>
> Prefer `combinable` for a single numeric accumulator; prefer ETS when `T` is
> non-default-constructible, stateful, or you need to enumerate per-thread buffers.

**Under the hood ▸** `parallel_reduce` internally uses a thread-local reduction
pattern equivalent to ETS/combinable — you get the same scalability without
managing the object when a single range fold suffices (Part 1.2). Reach for ETS
when the pattern repeats, when `T` is complex, or when you are **not** inside a
reduce-shaped algorithm.

---

## 5.3.5 Pitfalls and tuning

> **Pitfall ▸** Calling `local()` **outside** a parallel region always returns
> the **calling thread's** copy — usually the main thread's. Initialize and
> combine inside the parallel structure, not from `main` mid-run.

> **Pitfall ▸** ETS creates one copy per **thread that calls `local()`**, not
> per task. With work-stealing a thread may process many tasks — that is
> exactly what you want (one accumulator per worker). Do not assume one copy per
> task.

**Trade-offs ▸** Merge at the end is serial. For P ≪ N this is negligible; for
very expensive merge functions, consider tree reduction or `parallel_reduce`.
Memory: P copies of `T` — fine for scalars and moderate vectors, costly if `T`
is huge and many idle threads touch `local()` once.

**Tuning ▸** Pair ETS with **`scalable_allocator`** inside thread-local vectors
(Part 5.1) when local buffers grow dynamically. For read-mostly aggregation into
fixed arrays, `cache_aligned_allocator` on a small per-thread array (Part 5.2)
can beat ETS overhead when thread count is known and fixed.

---

## Summary

- **`enumerable_thread_specific<T>`** gives each worker a **lock-free local
  copy** of `T`, lazily created on first `local()`.
- Use **`combine()`** / **`combine_each()`** or range-for to fold per-thread
  state after parallel work — the standard fix for shared-write bottlenecks.
- **`ets_key_usage_type`** controls keying; default `ets_no_key` suffices for
  most per-thread accumulation.
- Prefer **`parallel_reduce`** for one-shot folds; **`combinable<T>`** for simple
  lazy locals; **ETS** for complex, reusable, enumerable thread-local state.
- ETS is the pattern behind scalable reductions — eliminate synchronization in
  the hot path, pay a cheap serial merge at the end.

Next: [6.1 — Flow Graph fundamentals](../06-flow-graph/01-flow-graph-intro.md)
