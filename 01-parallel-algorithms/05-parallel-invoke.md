# 1.5 — parallel_invoke

`parallel_invoke` runs **2 to N** callable objects **concurrently** and returns only after
**all** complete. It is the structured fork-join primitive for **heterogeneous** tasks —
different functions, different inputs — without manually managing threads.

```
   caller ──┬──▶ task f0 ──┐
            ├──▶ task f1 ──├── wait ──▶ continue
            └──▶ task f2 ──┘
```

Think of it as a barrier-synchronized fork: ideal for divide-and-conquer with a small,
fixed fan-out at each recursion level.

---

## 1.5.1 Semantics

> **The API ▸**
> ```cpp
> // <oneapi/tbb/parallel_invoke.h>
> template<typename F0, typename F1>
> void parallel_invoke(F0&& f0, F1&& f1);
> template<typename F0, typename F1, typename F2, typename... Rest>
> void parallel_invoke(F0&& f0, F1&& f1, F2&& f2, Rest&&... rest);
> ```
> **Semantics:** Spawns one task per function (except one may run on the calling worker).
> Blocks until every function returns. If any function throws, the exception propagates
> from `parallel_invoke` after others finish (aggregate exception behavior per oneTBB version).

Requirements:

```
   ✓  Functions are independent (no data races on shared writes)
   ✓  Small fan-out (2–4 typical); large fan-out → consider parallel_for or task_group
   ✗  Dependent pipeline stages (use parallel_pipeline, Part 1.6)
```

---

## 1.5.2 Fork-join for divide-and-conquer

Classic pattern: split problem in half, recurse in parallel:

```
            sort(lo, hi)
                 │
         partition pivot
           /            \
    sort(lo, mid)    sort(mid+1, hi)
         │                  │
    parallel_invoke( left, right )
```

Below a **serial threshold**, run sequentially to avoid task overhead (same grainsize
intuition as Part 0.4).

> **Under the hood ▸** Each `parallel_invoke` creates a `task_group`-like join point
> internally. Nested `parallel_invoke` composes on the shared scheduler — no thread
> explosion (Part 0.1).

---

## 1.5.3 Comparison to `task_group`

| | `parallel_invoke` | `task_group` |
|---|-------------------|--------------|
| Arity | Fixed at call site (2..N) | Dynamic `run()` in a loop |
| Join | Implicit (blocking return) | Explicit `wait()` |
| Use case | Known branches (quicksort, 2-way merge) | Tree search, thread pool patterns |
| Cancellation | No | `cancel()` / `is_canceling()` (Part 2.1) |

```cpp
// parallel_invoke — fixed two-way fork
tbb::parallel_invoke(
    [&] { left(); },
    [&] { right(); });

// task_group — dynamic fan-out
tbb::task_group g;
for (int i = 0; i < n; ++i)
    g.run([&, i] { work(i); });
g.wait();
```

**Trade-offs ▸** `parallel_invoke` is clearer for static decomposition; `task_group` wins
when the number of branches is unknown at compile time or you need cancellation.

---

## 1.5.4 Recursive merge-sort style example

```cpp
// g++ -std=c++17 -O2 parallel_invoke_demo.cpp -ltbb
#include <oneapi/tbb/parallel_invoke.h>
#include <algorithm>
#include <cstdio>
#include <vector>

template<typename T>
void parallel_quicksort(std::vector<T>& v, ptrdiff_t lo, ptrdiff_t hi) {
    if (hi - lo < 4096) {
        std::sort(v.begin() + lo, v.begin() + hi + 1);
        return;
    }

    T pivot = v[(lo + hi) / 2];
    ptrdiff_t i = lo, j = hi;
    while (i <= j) {
        while (v[i] < pivot) ++i;
        while (v[j] > pivot) --j;
        if (i <= j) {
            std::swap(v[i], v[j]);
            ++i;
            --j;
        }
    }

    tbb::parallel_invoke(
        [&] { parallel_quicksort(v, lo, j); },
        [&] { parallel_quicksort(v, i, hi); });
}

int main() {
    const int n = 5'000'000;
    std::vector<int> data(n);
    for (int i = 0; i < n; ++i)
        data[i] = (i * 7919) % n;

    parallel_quicksort(data, 0, n - 1);

    const bool ok = std::is_sorted(data.begin(), data.end());
    std::printf("parallel quicksort %s  (n=%d)\n", ok ? "✓" : "✗", n);
    return ok ? 0 : 1;
}
```

For production sorting, prefer `tbb::parallel_sort` (Part 1.4) — this example shows
**recursive fork-join** mechanics, not the fastest sort.

---

## 1.5.5 Heterogeneous parallel startup

Independent initialization of subsystems:

```cpp
Database db;
Mesh mesh;
Config cfg;

tbb::parallel_invoke(
    [&] { db.connect("localhost"); },
    [&] { mesh.load("model.obj"); },
    [&] { cfg.parse("app.toml"); });
// all three ran concurrently; continue when all ready
```

> **Pitfall ▸** Captured references to objects each task mutates independently is fine;
> capturing one object **all tasks write** is a race. Each branch should own distinct
> output state or use synchronization.

---

## Summary

- `parallel_invoke` runs **2..N independent functions** in parallel and **joins** before
  returning.
- Best for **fixed fan-out** fork-join (divide-and-conquer, parallel init).
- **`task_group`** is the dynamic, cancellable alternative (Part 2.1).
- Use a **serial cutoff** to limit task overhead at small problem sizes.
- Nested invocations compose safely on the oneTBB scheduler.

Next: [1.6 — parallel_pipeline](06-parallel-pipeline.md)
