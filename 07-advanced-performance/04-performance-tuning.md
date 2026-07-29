# 7.4 — Performance Tuning & Amdahl's Law

Parts 0.4 and 0.5 gave you grain size and partitioners. Part 5 gave you allocators
and false-sharing fixes. This chapter ties them into a **measurement-driven tuning
loop** — and explains why a tiny serial fraction caps speedup no matter how many
cores you throw at the problem.

---

## 7.4.1 Amdahl's law — the serial ceiling

Gene Amdahl (1967): if fraction **S** of a program must run serially, infinite
cores still leave at least **S** of the runtime sequential. Speedup on **P** workers:

```
   speedup(P) = 1 / ( S + (1 − S) / P )

   S = serial fraction (setup, locks, single-thread phases, I/O waits in hot path)
   (1 − S) = parallelizable fraction
```

![Speedup vs cores for different serial fractions — even 5% serial caps near 9× on 16 cores](figures/speedup-amdahl.svg)

```
   S = 5% serial  →  max speedup ≈ 1/0.05 = 20×  (theoretical)
                    at P=16:  1 / (0.05 + 0.95/16) ≈ 8.4×

   S = 1% serial  →  at P=16:  ≈ 12.3×
   S = 0% ideal   →  linear until memory bandwidth / overhead
```

Even **5% serial** cuts 16-core efficiency roughly in half versus linear scaling.
Before tuning grain size, find **S** — profile the serial tail.

> **The key idea ▸** Parallel algorithms shrink **(1 − S)**. They cannot shrink
> **S**. A mutex around every iteration, a single global reduction lock, or one
> giant setup pass dominates at scale.

**Gustafson's law (brief):** if problem size grows with P, you may see **near-linear**
"scaled speedup" even with serial overhead — because the parallel part dominates
total work. Strong scaling (fixed N, more cores) still hits Amdahl; weak scaling
(larger N per core) often looks better. Report which you measured.

---

## 7.4.2 The tuning checklist

Work through this list in order — measure after each change:

```
   ┌────────────────┬─────────────────────────────────────────────────────────┐
   │ 1. Correctness │ No races (Part 7.5); use subrange r, not full array     │
   │ 2. Decompose   │ Independent iterations? Right algorithm (for/reduce/    │
   │                │ scan/pipeline — Part 1)?                                │
   │ 3. Grain size  │ Task time ≫ spawn cost (~1–10 µs per task target)       │
   │                │ Part 0.4: start 1000–10000 iterations for light bodies    │
   │ 4. Partitioner │ uniform → static/simple; variable → auto (Part 0.5)     │
   │ 5. False share │ Separate cache lines per thread (Part 5.2)                │
   │ 6. Allocator   │ tbbmalloc / scalable_allocator for hot parallel allocs    │
   │                │ (Part 5.1)                                                │
   │ 7. Sync        │ Minimize locks; prefer atomics, concurrent containers,    │
   │                │ thread-local accumulation + one merge (Part 3, 4)         │
   │ 8. Concurrency │ global_control / task_arena — no oversubscription (7.1)   │
   │ 9. NUMA        │ First-touch on socket-local memory; pin if measured win   │
   └────────────────┴─────────────────────────────────────────────────────────┘
```

**Tuning ▸** **Start with defaults.** `parallel_for` + `auto_partitioner` is often
within 90% of optimal. Profile before micro-tuning grain — wrong grain is a common
second-order mistake after a hidden serial bottleneck.

---

## 7.4.3 Grain size and task overhead

Each subrange below grainsize becomes at most one task. Too small → millions of
tasks → deque traffic, steals, cache misses:

```
   iterations per task  │  typical outcome
   ─────────────────────┼──────────────────────────────────
   1–10                 │  ✗ slower than serial (overhead wins)
   1'000–10'000 (light) │  ✓ good starting band
   10'000+ (heavy body) │  ✓ fewer tasks, still enough parallelism
```

Rule of thumb: aim for **≥ 1–10 microseconds** of useful work per task body.
Measure: halve/double grain and compare wall time at fixed thread count.

---

## 7.4.4 Measuring speedup and efficiency

```
   speedup(P)     = T_serial / T_parallel(P)
   efficiency(P)  = speedup(P) / P     (100% = perfect linear strong scaling)

   strong scaling: fixed problem size N, increase P
   weak scaling:   N ∝ P (each core gets same chunk size)
```

Benchmark hygiene:

- Warm up caches; run multiple trials; report median.
- Pin `global_control` thread count outside the timed region (Part 7.1).
- Disable turbo variance if comparing across commits (where possible).
- Log **both** wall time and CPU time — I/O shows up only in wall.

> **Pitfall ▸** Comparing "1 thread" as `parallel_for` with 1 worker vs a plain
> `for` loop unfairly charges TBB overhead. Use serial `for` as baseline, or
> `task_arena(1).execute(...)` for apples-to-apples.

---

## 7.4.5 Profiling — find S before tuning grain

| Tool | What it shows |
|------|----------------|
| **Intel VTune** | Hotspots, threading, TBB overhead, cache misses, NUMA |
| **Linux perf** | `perf record -g`, `perf stat` — CPI, LLC misses, call stacks |
| **TBB observers** | Worker entry counts, custom timers (Part 7.2) |

Look for:

```
   ✗ One thread at 100% while others idle     → load imbalance or serial section
   ✗ High spin time in mutex                  → sync bottleneck (Part 4)
   ✗ cache-misses on distinct writes          → false sharing (Part 5.2)
   ✗ tbb:: internal steal / wait dominance    → grains too fine or too few tasks
   ✗ malloc in parallel hot loop              → switch allocator (Part 5.1)
```

VTune's **Threading / Locks and Waits** view maps directly to Amdahl's **S**.

---

## 7.4.6 Oversubscription and NUMA

**Oversubscription:** more runnable threads than cores → OS time-slicing → cache
cold, context switches. TBB avoids this when nested parallelism shares one pool
(Part 0.1). It returns if you mix TBB with OpenMP, `std::thread`, or multiple
runtimes each claiming all cores — cap with **`global_control`** or isolate with
**`task_arena`**.

**NUMA:** on multi-socket machines, memory attached to a remote socket costs 2–3×
latency. **First-touch** policy means parallel initialization determines placement:

```cpp
// g++ -std=c++17 -O2 numa_touch.cpp -ltbb
#include <oneapi/tbb/parallel_for.h>
#include <oneapi/tbb/blocked_range.h>
#include <vector>

void touch_in_parallel(std::vector<double>& v) {
    tbb::parallel_for(
        tbb::blocked_range<std::size_t>(0, v.size()),
        [&](const tbb::blocked_range<std::size_t>& r) {
            for (std::size_t i = r.begin(); i != r.end(); ++i)
                v[i] = 0.0;  // first touch on the worker's socket
        });
}
```

Combine with **`task_scheduler_observer`** pinning (Part 7.2) only after profiling
proves benefit — blind pinning often hurts.

---

## 7.4.7 A minimal tuning sweep

```cpp
// g++ -std=c++17 -O2 tuning_sweep.cpp -ltbb
#include <oneapi/tbb/global_control.h>
#include <oneapi/tbb/parallel_for.h>
#include <oneapi/tbb/blocked_range.h>
#include <oneapi/tbb/partitioner.h>
#include <chrono>
#include <cstdio>
#include <thread>
#include <vector>

template<typename F>
long long ms(F&& f) {
    auto t0 = std::chrono::steady_clock::now();
    f();
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - t0).count();
}

int main() {
    const int N = 50'000'000;
    std::vector<double> a(N, 1.0);

    auto serial = [&] {
        for (int i = 0; i < N; ++i) a[i] = a[i] * 1.001 + 0.001;
    };
    auto parallel = [&](int grain, bool use_auto) {
        if (use_auto) {
            tbb::parallel_for(tbb::blocked_range<int>(0, N, grain),
                [&](const tbb::blocked_range<int>& r) {
                    for (int i = r.begin(); i != r.end(); ++i)
                        a[i] = a[i] * 1.001 + 0.001;
                });
        } else {
            tbb::parallel_for(
                tbb::blocked_range<int>(0, N, grain),
                [&](const tbb::blocked_range<int>& r) {
                    for (int i = r.begin(); i != r.end(); ++i)
                        a[i] = a[i] * 1.001 + 0.001;
                },
                tbb::static_partitioner{});
        }
    };

    const long long t_serial = ms(serial);
    std::printf("serial: %lld ms\n", t_serial);

    tbb::global_control gc(tbb::global_control::max_allowed_parallelism,
        static_cast<int>(std::thread::hardware_concurrency()));

    for (int grain : {1000, 10000, 100000}) {
        long long tp = ms([&] { parallel(grain, true); });
        std::printf("grain=%d auto: %lld ms  speedup=%.2f\n",
            grain, tp, static_cast<double>(t_serial) / tp);
    }

    return 0;
}
```

Sweep grain and partitioner; plot speedup vs P with fixed `global_control`. If
speedup plateaus below cores, profile for **S** — not smaller grains.

---

## Summary

- **Amdahl's law:** serial fraction **S** caps speedup — even 5% serial limits
  ~16-core gains to roughly **8×**; find **S** with a profiler before micro-tuning.
- **Checklist:** correctness → decomposition → grain → partitioner → false sharing
  → allocator → synchronization → concurrency caps → NUMA.
- **Measure** speedup and efficiency; distinguish strong vs weak scaling; use serial
  `for` as baseline.
- **VTune / perf** expose hotspots, lock wait, and cache effects; TBB observers
  augment worker-level insight.
- Avoid **oversubscription** across runtimes; use **first-touch** and measured
  pinning for NUMA.

Next: [7.5 — Common pitfalls](05-common-pitfalls.md)
