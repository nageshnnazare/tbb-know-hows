# 5.1 — The Scalable Allocator

Part 0.1 placed **tbbmalloc** at the bottom of the TBB stack — the layer that
feeds every task allocation. This chapter explains why the default `malloc`/`new`
becomes a serial bottleneck under many threads, how tbbmalloc's per-thread heaps
eliminate the fast-path lock, and the three ways you plug it in: explicit
`scalable_allocator<T>`, the lighter `tbb_allocator<T>`, and the transparent
**tbbmalloc_proxy** shim.

---

## 5.1.1 Why default malloc serializes

Most system allocators (`glibc malloc`, `jemalloc` in some configs) protect a
global heap with a **central lock** (or a small set of arena locks). Under light
contention this is fine; under a `parallel_for` where every task allocates a
temporary `std::vector`, every `push_back` may hit the same lock:

```
   thread 0 ──malloc──▶ [ GLOBAL HEAP LOCK ] ◀──malloc── thread 1
   thread 2 ──free────▶       ▲              ◀──malloc── thread 3
                              │
                    one winner at a time
                    → allocator-bound, not compute-bound
```

The symptom is unmistakable in a profile: `malloc`, `_int_malloc`, or
`__libc_calloc` at the top of the hot path, and **speedup that stalls** once
thread count exceeds a handful — even when your algorithm body is embarrassingly
parallel.

> **Under the hood ▸** tbbmalloc maintains **per-thread memory pools**. A worker
> that allocates in a hot loop draws from its own cache of free blocks; the
> fast path is **lock-free**. Blocks are returned to a global pool only when a
> thread's local cache overflows or at thread exit. Cross-thread `free` of a
> block allocated on another thread is slower (remote deallocation) but still
> avoids hammering one global mutex on every allocation.

---

## 5.1.2 The allocator landscape

![Per-thread tbbmalloc pools vs a single global malloc lock under many workers](figures/scalable-allocator.svg)

```
   std::allocator / malloc          scalable_allocator / tbbmalloc
   ───────────────────────          ──────────────────────────────
   global lock on hot path          per-thread pool, lock-free fast path
   scales poorly with threads       designed for P concurrent allocators
   no code changes                  template param or link flag
```

TBB gives you three integration points, from most explicit to fully transparent:

| Mechanism | How you enable it | Best for |
|-----------|-------------------|----------|
| `scalable_allocator<T>` | template parameter on containers | hot loops you control |
| `tbb_allocator<T>` | same; uses scalable heap when linked | general TBB code |
| `tbbmalloc_proxy` | link `-ltbbmalloc_proxy` or `LD_PRELOAD` | legacy/third-party code |

Part 5.2 covers **layout** (false sharing); this chapter covers **allocation
contention** — a separate but equally common reason parallel code fails to scale.

---

## 5.1.3 scalable_allocator and tbb_allocator

Both are STL-compatible allocators. Use them wherever you would write
`std::allocator<T>`:

> **The API ▸**
>
> ```cpp
> #include <oneapi/tbb/scalable_allocator.h>
> #include <oneapi/tbb/tbb_allocator.h>
>
> template<typename T> using scalable = tbb::scalable_allocator<T>;
> template<typename T> using tbb_alloc = tbb::tbb_allocator<T>;
>
> std::vector<int, scalable<int>> v;
> std::list<double, tbb_alloc<double>> lst;
> ```
>
> `scalable_allocator<T>` routes directly through tbbmalloc. `tbb_allocator<T>`
> is the general-purpose TBB allocator (scalable when `-ltbbmalloc` is linked).
> Link: `g++ ... -ltbb -ltbbmalloc`.

```cpp
// build: g++ -std=c++17 -O2 alloc_bench.cpp -ltbb -ltbbmalloc -o alloc_bench
#include <oneapi/tbb/parallel_for.h>
#include <oneapi/tbb/blocked_range.h>
#include <oneapi/tbb/scalable_allocator.h>
#include <chrono>
#include <cstdio>
#include <vector>

template<typename Alloc>
double bench(const char* label) {
    using Vec = std::vector<int, Alloc>;
    auto t0 = std::chrono::steady_clock::now();
    tbb::parallel_for(tbb::blocked_range<int>(0, 2000),
        [&](const tbb::blocked_range<int>& r) {
            for (int i = r.begin(); i != r.end(); ++i) {
                Vec tmp(512);          // allocate + touch every iteration
                tmp[0] = i;
            }
        });
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - t0).count();
    std::printf("%s: %lld ms\n", label, static_cast<long long>(ms));
    return static_cast<double>(ms);
}

int main() {
    double std_ms  = bench<std::allocator<int>>("std::allocator");
    double tbb_ms  = bench<tbb::scalable_allocator<int>>("scalable_allocator");
    std::printf("ratio (std/tbb): %.2fx\n", std_ms / tbb_ms);
    return 0;
}
```

On a many-core machine with allocation-heavy bodies, the scalable run is often
2–10× faster — not because tbbmalloc is magic, but because it removes the
**serial choke point** the default allocator becomes under contention.

**Trade-offs ▸** Per-thread caches trade memory for speed: RSS may rise
compared to a frugal system allocator. Remote frees (thread A frees memory
allocated by thread B) still synchronize. For **zero** allocation in the hot
path — pre-sized buffers, object pools — neither allocator helps; see Part 5.3
(`enumerable_thread_specific`) for per-thread reuse without locks.

---

## 5.1.4 tbbmalloc_proxy — transparent replacement

When you cannot change source — a third-party library, legacy `new`/`delete`
everywhere — link **tbbmalloc_proxy**. It interposes on `malloc`, `free`,
`operator new`, and `operator delete` at load time:

```
   your binary  →  libtbbmalloc_proxy.so  →  tbbmalloc  →  OS
                      (intercepts malloc/free/new/delete)
```

> **The API ▸**
>
> ```bash
> # compile-time (Linux/macOS)
> g++ -std=c++17 -O2 app.cpp -ltbbmalloc_proxy -ltbbmalloc -ldl -o app
>
> # run-time injection (no recompile)
> LD_PRELOAD=libtbbmalloc_proxy.so ./app          # Linux
> DYLD_INSERT_LIBRARIES=libtbbmalloc_proxy.dylib ./app  # macOS
> ```
>
> Install: `apt install libtbbmalloc2` / `brew install tbb`. The proxy library
> name varies by platform (`libtbbmalloc_proxy.so`, `.dylib`, `.dll`).

**Trade-offs ▸** Global interposition affects *every* allocation in the process —
including libraries that may assume glibc's behavior. Test thoroughly. Some
allocators (ASan-instrumented builds, custom pool allocators) conflict with
proxy interposition. Prefer explicit `scalable_allocator` where you control the
hot path.

> **Pitfall ▸** Linking `-ltbb` alone does **not** replace `malloc`. You need
> `-ltbbmalloc` for explicit allocators, or `-ltbbmalloc_proxy` for transparent
> replacement. Forgetting this and wondering why `scalable_allocator` "does
> nothing" on a system without tbbmalloc installed is a common deployment miss.

---

## 5.1.5 Measuring allocation contention

Do not guess — profile. Three practical approaches:

```
   1. TIME vs THREADS     run the same benchmark at 1, 2, 4, 8, … cores
                          flat or inverted curve → suspect allocator or false sharing

   2. perf lock analysis  perf record -g ./app && perf report
                          hot symbols: malloc, _int_free, tbb::internal::...

   3. VTune / heap stats  "Memory Bound" + "Contention" views show lock wait
                          on allocator internals
```

**Tuning ▸** If allocation dominates:

1. Switch hot containers to `scalable_allocator<T>` (smallest code change).
2. If third-party code allocates heavily, try `-ltbbmalloc_proxy`.
3. Reduce allocation **frequency**: reserve capacity, reuse buffers via
   `enumerable_thread_specific` (Part 5.3), move allocation outside
   `parallel_for`.
4. Distinguish allocator contention from **false sharing** (Part 5.2) — both
   produce "more threads, no faster" but the fixes differ.

```cpp
// Quick A/B: same body, two allocators — run under `perf stat -e cache-misses,cycles`
tbb::parallel_for(range, [&](auto r) {
    std::vector<int> a(r.size());              // std allocator → lock contention
    // vs
    std::vector<int, tbb::scalable_allocator<int>> b(r.size());  // per-thread pools
    (void)a; (void)b;
});
```

---

## Summary

- Default `malloc`/`new` serializes on a **global heap lock** when many threads
  allocate concurrently — a hidden bottleneck beneath otherwise parallel code.
- **tbbmalloc** uses **per-thread pools** with a lock-free fast path; cross-thread
  frees are slower but rare in well-structured per-thread allocation patterns.
- **`scalable_allocator<T>`** and **`tbb_allocator<T>`** are drop-in STL
  replacements; link **`-ltbbmalloc`**.
- **`tbbmalloc_proxy`** transparently replaces `malloc`/`free`/`new`/`delete` via
  link flag or `LD_PRELOAD` — useful for code you cannot modify.
- Measure with scaling curves and profilers; pair allocator fixes with layout
  fixes (Part 5.2) when contention persists.

Next: [5.2 — Cache alignment & false sharing](02-cache-alignment-false-sharing.md)
