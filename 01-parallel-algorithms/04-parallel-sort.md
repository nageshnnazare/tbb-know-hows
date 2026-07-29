# 1.4 — parallel_sort

`tbb::parallel_sort` is a drop-in parallel replacement for `std::sort` on random-access
ranges. It is an **unstable**, **comparison-based** parallel quicksort: partitions spawn
as tasks on the work-stealing scheduler (Part 0.3), with automatic cutoff to serial sort
on small subranges.

```
   parallel_sort(begin, end)
        │
        ├─ partition around pivot ──▶ left half  (task)
        │                           right half (task or steal)
        └─ subranges ≤ cutoff → std::sort locally
```

No grain-size knob is exposed — the implementation picks task granularity internally.

---

## 1.4.1 Algorithm and requirements

> **The API ▸**
> ```cpp
> // <oneapi/tbb/parallel_sort.h>
> template<typename RandomAccessIterator, typename Compare>
> void parallel_sort(RandomAccessIterator begin, RandomAccessIterator end,
>                    Compare comp);
> template<typename RandomAccessIterator>
> void parallel_sort(RandomAccessIterator begin, RandomAccessIterator end);
> ```
> **Semantics:** Sorts `[begin, end)` in ascending order by default (`std::less`). Mutates
> in place. **Not stable** — equal elements may reorder.

**Iterator requirements:** random access (`it + n`, O(1) `[]`). Pointers, `std::vector`,
`std::deque` iterators qualify; `std::list` does not.

**Comparator:** must define a **strict weak ordering**, same as `std::sort`.

---

## 1.4.2 Unstable quicksort behavior

`parallel_sort` is parallel quicksort, not mergesort. Implications:

```
   ✓  O(n log n) average time, good multicore speedup on large n
   ✗  Not stable — use parallel_stable_sort if you need stability (when available)
   ✗  Worst-case O(n²) on adversarial input (same as naive quicksort; rare in practice)
```

Stability example:

```cpp
struct Item { int key; int seq; };
// Two items with key=1 may swap order after parallel_sort by key
```

For stable parallel sort, Intel provides `parallel_stable_sort` in some releases; check
your oneTBB version. Otherwise stable sort is serial or a custom key-index sort.

---

## 1.4.3 When it beats `std::sort`

| Scenario | Winner |
|----------|--------|
| `n < ~10⁴`, few cores | Often `std::sort` (parallel overhead dominates) |
| `n ≥ 10⁶`, 8+ cores | `parallel_sort` typically 2–4× faster |
| Sorting once at startup | Either; measure |
| Hot loop sorting repeatedly | `parallel_sort`; consider sorting indices instead |

**Tuning ▸** You cannot tune grainsize directly. If sort is a bottleneck, consider
**sorting an index vector** (`O(n log n)` compares on integers) or **radix sort** for
fixed-width keys outside TBB.

> **Under the hood ▸** Subrange tasks are spawned until a size threshold, then
> `std::sort` / `std::partition` runs serially — same "leaf serial, trunk parallel"
> pattern as `parallel_for`.

---

## 1.4.4 Comparator and types

Any move-assignable element type works. Custom comparators enable multi-field keys:

```cpp
tbb::parallel_sort(items.begin(), items.end(),
    [](const Item& a, const Item& b) {
        if (a.key != b.key) return a.key < b.key;
        return a.seq < b.seq;   // tie-break (still unstable for equal key+seq pairs!)
    });
```

**Pitfall ▸** Capturing non-thread-safe state in a comparator used during parallel
partition is a data race. Comparators must be pure functions of their arguments.

---

## 1.4.5 Example: sort a large vector

```cpp
// g++ -std=c++17 -O2 parallel_sort_demo.cpp -ltbb
#include <oneapi/tbb/parallel_sort.h>
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <random>
#include <vector>

int main() {
    const size_t n = 50'000'000;
    std::vector<int> data(n);

    std::mt19937 rng(42);
    std::uniform_int_distribution<int> dist(0, 1'000'000'000);
    for (size_t i = 0; i < n; ++i)
        data[i] = dist(rng);

    auto t0 = std::chrono::steady_clock::now();
    tbb::parallel_sort(data.begin(), data.end());
    auto t1 = std::chrono::steady_clock::now();

    const bool ok = std::is_sorted(data.begin(), data.end());
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count();

    std::printf("sorted %zu ints in %lld ms  %s\n",
                n, static_cast<long long>(ms), ok ? "✓" : "✗");
    std::printf("first=%d  last=%d\n", data.front(), data.back());
    return ok ? 0 : 1;
}
```

Descending order:

```cpp
tbb::parallel_sort(data.begin(), data.end(), std::greater<int>{});
```

**Trade-offs ▸** `parallel_sort` is the simplest parallel sort API but gives no control
over memory (in-place), stability, or worst-case guarantees. For pipeline integration with
ordered output stages, consider whether unstable reorder matters downstream (Part 1.6).

---

## 1.4.6 Relation to the C++ parallel STL

On many platforms, `std::sort(std::execution::par, ...)` delegates to oneTBB internally.
`parallel_sort` remains useful when you want a **direct** dependency without `<execution>`,
identical behavior across TBB versions, or co-location with other TBB types in the same
translation unit. Neither API is stable; both require random-access iterators and a valid
comparator.

```
   std::sort(par, begin, end)     ← standard facade, implementation-defined backend
   tbb::parallel_sort(begin, end)  ← explicit TBB, same scheduler as your parallel_for
```

If the sort is one step in a larger TBB pipeline, keeping **`tbb::parallel_sort`** avoids
mixing executors and makes profiling with TBB-aware tools simpler (Part 7.4).

---

## Summary

- `parallel_sort` is **unstable parallel quicksort** over **random-access** iterators.
- API mirrors `std::sort`: range + optional **comparator**.
- Wins on **large n** and **many cores**; small arrays often favor serial sort.
- **Not stable**; equal keys may permute across runs.
- Comparator must be thread-safe and define a strict weak ordering.

Next: [1.5 — parallel_invoke](05-parallel-invoke.md)
