# 5.2 — Cache Alignment & False Sharing

Part 5.1 removed the **allocator** bottleneck. This chapter tackles a subtler
layout problem: two threads writing **independent** variables that happen to
live on the **same cache line**, forcing the line to ping-pong between cores until
parallel code runs **slower than serial**. We cover what a cache line is, how to
detect false sharing, and the fixes — `cache_aligned_allocator`, `alignas`,
padding, and per-thread accumulators.

---

## 5.2.1 Cache lines and coherency

CPUs do not move individual bytes between cores. The unit of transfer and
**cache coherency** is the **cache line** — typically **64 bytes** on x86-64 and
Apple Silicon (always verify with `std::hardware_destructive_interference_size`
in C++17):

```
   ┌──────────────────────────── 64 B cache line ────────────────────────────┐
   │ counter[0] (8 B) │ counter[1] (8 B) │ counter[2] ... │  (padding)       │
   └──────────────────────────────────────────────────────────────────────────┘
        ▲ core 0 writes here              ▲ core 1 writes here
        └──────── same line ──────────────┘
              → MESI invalidation traffic on every increment
```

When core 0 writes byte 0, the hardware marks the entire line **Modified** in
core 0's cache and **Invalid** everywhere else. Core 1's next write to byte 8
(on the *same* line) must fetch the line exclusive again — even though the two
threads touch **different logical variables**. That is **false sharing**: real
contention on imaginary sharing.

> **Under the hood ▸** The MESI (Modified / Exclusive / Shared / Invalid)
> protocol keeps caches coherent at line granularity. A write is always a
> read-for-ownership of the whole line. No amount of "my variables are logically
> independent" helps if they share a line — the hardware cannot tell intent from
> layout.

---

## 5.2.2 The performance cliff

![Two cores writing adjacent counters on one cache line — the line ping-pongs between L1 caches](figures/false-sharing.svg)

False sharing produces a signature scaling curve:

```
   speedup
      │        ideal ╱
      │             ╱
      │   actual ──╱───────  (flat or WORSE with more threads)
      └──────────────────────── threads
```

A 16-core run **slower than 1-core** almost always means layout or allocator
contention (Part 5.1), not "parallelism doesn't work."

**Detection tools:**

| Tool | What to look for |
|------|------------------|
| `perf c2c` (Linux) | HITM (hit modified) on the same cache line from different cores |
| Scaling benchmark | throughput drops as threads increase despite independent writes |
| VTune Microarchitecture | elevated `OFFCORE_RESPONSE` / cache-line invalidation |

> **Pitfall ▸** `std::atomic` prevents **data races** but does **not** prevent
> false sharing. Two atomics on the same cache line still invalidate each other
> on every RMW. Alignment fixes the layout; atomics fix correctness — you often
> need both.

---

## 5.2.3 Before and after: per-thread counters

The classic failure mode: an array of counters, one per thread, packed tightly:

```cpp
// build: g++ -std=c++17 -O2 false_sharing.cpp -ltbb -o false_sharing
#include <oneapi/tbb/parallel_for.h>
#include <oneapi/tbb/blocked_range.h>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <vector>

struct Counter { std::atomic<long long> value{0}; };  // ~8 B → many fit per line

void run(const char* label, auto make_counters) {
    auto counters = make_counters();
    const int n = static_cast<int>(counters.size());
    const long long iters = 5'000'000;

    auto t0 = std::chrono::steady_clock::now();
    tbb::parallel_for(tbb::blocked_range<int>(0, n),
        [&](const tbb::blocked_range<int>& r) {
            for (int t = r.begin(); t != r.end(); ++t)
                for (long long i = 0; i < iters / n; ++i)
                    counters[t].value.fetch_add(1, std::memory_order_relaxed);
        });
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - t0).count();
    long long sum = 0;
    for (auto& c : counters) sum += c.value.load();
    std::printf("%s: %lld ms  sum=%lld\n", label, static_cast<long long>(ms), sum);
}

int main() {
    const int threads = 8;

    // BEFORE: counters packed → false sharing
    run("unaligned", [&] {
        return std::vector<Counter>(threads);
    });

    // AFTER: each counter on its own cache line
    struct alignas(64) AlignedCounter { std::atomic<long long> value{0}; };
    run("alignas(64)", [&] {
        return std::vector<AlignedCounter>(threads);
    });

    return 0;
}
```

On a typical 8-core machine the aligned version is **5–20× faster** for this
micro-benchmark — same algorithm, same atomics, different layout.

---

## 5.2.4 Fixes

### cache_aligned_allocator

> **The API ▸**
>
> ```cpp
> #include <oneapi/tbb/cache_aligned_allocator.h>
> std::vector<T, tbb::cache_aligned_allocator<T>> v(n);
> ```
>
> Every element starts on a cache-line boundary. Use for arrays of per-thread
> accumulators, row sums, or any structure multiple workers write independently.

Combines naturally with `scalable_allocator` concerns: alignment for layout,
scalable for allocation speed (Part 5.1).

### alignas and hardware_destructive_interference_size

C++17 gives a portable minimum spacing:

```cpp
struct alignas(std::hardware_destructive_interference_size) Padded {
    std::atomic<int> counter{0};
    // compiler adds tail padding to fill the line
};
```

Prefer `std::hardware_destructive_interference_size` over hard-coded `64` when
available; fall back to `64` on older toolchains.

### Manual padding

When you cannot change allocation:

```cpp
struct Counter {
    std::atomic<long long> value{0};
    char pad[64 - sizeof(std::atomic<long long>)];  // pad to one line
};
```

Fragile (line size varies) but explicit and common in low-level code.

### Per-thread accumulators (best pattern)

Instead of `counters[thread_id]++`, accumulate locally and merge once:

```cpp
// local int sum in parallel_reduce body — zero sharing
// or enumerable_thread_specific (Part 5.3) for reusable per-thread state
```

This eliminates sharing entirely — better than padding an shared array.

**Trade-offs ▸** Cache-line alignment **wastes memory**: an `int` padded to 64 B
uses 16× the nominal size. For millions of elements this matters; for a handful
of per-thread accumulators it is negligible. Profile before padding everything.

**Tuning ▸** False sharing often hides inside **node-based containers**
(`std::list`, tree nodes) where adjacent nodes share lines. Prefer array-of-
structures with alignment, or ETS for per-thread scratch. Read-mostly data
rarely false-shares — the problem is **writes** to the same line.

---

## Summary

- A **cache line** (~64 B) is the granularity of cache coherency; writes
  invalidate the **entire line**, not just the byte touched.
- **False sharing** occurs when independent variables share a line — cores
  ping-pong the line and parallel speedup collapses.
- Detect with **scaling curves**, `perf c2c`, or VTune; atomics alone do not
  fix layout.
- Fix with **`cache_aligned_allocator<T>`**, **`alignas(64)`** /
  `hardware_destructive_interference_size`, padding, or **per-thread local
  accumulators** (Part 5.3).
- Alignment trades memory for bandwidth; prefer local accumulation over padded
  shared arrays when possible.

Next: [5.3 — Thread-local storage (enumerable_thread_specific)](03-thread-local-storage.md)
