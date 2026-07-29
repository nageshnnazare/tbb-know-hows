# 4.2 — spin_mutex & queuing_mutex

Two exclusive mutexes bracket the contention spectrum: **`spin_mutex`** busy-
waits in user space for **vanishingly short** critical sections;
**`queuing_mutex`** also spins but queues waiters in **strict FIFO order** so
no thread starves under sustained hammering. Both use the same
[`scoped_lock`](01-mutexes.md) idiom from Part 4.1; the difference is **what
they do while waiting** and **who gets the lock next**.

---

## 4.2.1 spin_mutex: busy-wait mechanics

```
   thread A holds lock
   thread B: while (!try_acquire()) { /* spin */ }  ← burns CPU
   thread C: same spin loop                         ← burns CPU
```

> **The API ▸**
> ```cpp
> #include <oneapi/tbb/spin_mutex.h>
> tbb::spin_mutex m;
> tbb::spin_mutex::scoped_lock lock(m);
> ```

> **Under the hood ▸** Typically implemented with an atomic flag (test-and-set /
> compare-exchange). Waiters loop until the flag clears — **no syscall**, so
> acquire/release latency is nanoseconds if uncontended. Under contention, every
> waiter hammers the same cache line → **false sharing** and bus traffic
> ([Part 5.2](../05-memory/02-cache-alignment-false-sharing.md)).

**When spin_mutex wins**
- Critical section is a **few instructions** (increment, pointer swap, flag set).
- Contention is **low** — most attempts succeed on first try.
- Hold time **≪** OS scheduler quantum (~1 ms).

**When it loses**
- Section takes **microseconds or more** → use `tbb::mutex` (blocks, frees core).
- Many threads compete → spinning wastes **entire cores**; throughput collapses.
- **Fairness** matters → spin_mutex is **not fair**; one unlucky thread can
  wait arbitrarily long (starvation).

> **Pitfall ▸** Calling **I/O**, **malloc**, or **`parallel_for`** inside a
> `spin_mutex` critical section is an anti-pattern — hold time explodes and
> spinners waste all cores. Keep the section to **memory touches only**.

---

## 4.2.2 queuing_mutex: FIFO fairness

```
   wait queue:  [ B ] → [ C ] → [ D ]
                    ▲
   A releases ─────┘  B acquires next (FIFO)
```

> **The API ▸**
> ```cpp
> #include <oneapi/tbb/queuing_mutex.h>
> tbb::queuing_mutex m;
> tbb::queuing_mutex::scoped_lock lock(m);
> ```

> **Under the hood ▸** Waiters enqueue on a **linked queue**; unlock hands
> ownership to the head. Still spin-based waiting (no immediate OS block in
> classic implementation), but **acquisition order is deterministic** — first
> to request is first to acquire. Slightly higher overhead per lock/unlock than
> `spin_mutex`, but **scalable under contention** where spin_mutex thrashes.

**When queuing_mutex wins**
- Many threads repeatedly lock the **same** mutex.
- **Latency fairness** and **no starvation** are requirements (real-time-ish
  pipelines, per-thread accounting).
- Critical section still **short**, but not necessarily sub-microsecond.

**When it loses**
- **Uncontended** one-shot locks — plain `spin_mutex` or `mutex` may be cheaper.
- Very long critical sections — prefer `tbb::mutex` (OS block) so waiters don't spin.

---

## 4.2.3 Benchmark intuition (not gospel)

On a typical many-core CPU, rough ordering for **counter increment** workloads:

```
   uncontended, N small:     spin_mutex  ≤  queuing_mutex  <  mutex
   high contention, N huge:  queuing_mutex  often beats spin_mutex
                             (spin wastes cores; mutex syscall amortizes)
   hold time ~µs+:            mutex  wins (spinners and FIFO spin both costly)
```

Measure your **actual** critical section and contention — microbenchmarks lie
when the section does real work. Use `perf` / VTune (Part 7.4).

---

## 4.2.4 Example: scoped_lock with both types

```cpp
// g++ -std=c++17 -O2 spin_queuing_demo.cpp -ltbb
#include <oneapi/tbb/spin_mutex.h>
#include <oneapi/tbb/queuing_mutex.h>
#include <oneapi/tbb/parallel_for.h>
#include <oneapi/tbb/blocked_range.h>
#include <cstdio>

int main() {
    tbb::spin_mutex spin;
    tbb::queuing_mutex fair;
    int spin_count = 0;
    int fair_count = 0;

    tbb::parallel_for(
        tbb::blocked_range<int>(0, 100'000),
        [&](const tbb::blocked_range<int>& r) {
            for (int i = r.begin(); i != r.end(); ++i) {
                {
                    tbb::spin_mutex::scoped_lock lk(spin);
                    ++spin_count;   // tiny section — OK for spin_mutex
                }
                {
                    tbb::queuing_mutex::scoped_lock lk(fair);
                    ++fair_count;
                }
            }
        });

    std::printf("spin=%d fair=%d\n", spin_count, fair_count);
    return 0;
}
```

For a **read-heavy** shared config, neither mutex is ideal — see
[Reader-writer locks](03-reader-writer-locks.md). For **lock-free counters**,
see [Atomics](04-atomics.md).

**Tuning ▸** If profiling shows time in `_spin_wait`, either shorten the
section, shard data (per-thread accumulators + merge), or switch to
`queuing_mutex` / `mutex`. Align padded counters to cache lines (Part 5.2).

---

## 4.2.5 Side-by-side decision table

| Signal in profile / design | Action |
|----------------------------|--------|
| Lock hold time < ~20 ns, rare collision | `spin_mutex` |
| Many threads, same lock, short section, starvation bugs | `queuing_mutex` |
| Hold time > ~1 µs or includes allocation | `tbb::mutex` |
| Counter-only shared state | [`std::atomic`](04-atomics.md) or `parallel_reduce` |
| Append-only shared results | [`concurrent_vector`](../03-concurrent-containers/01-concurrent-vector.md) |

Neither `spin_mutex` nor `queuing_mutex` replaces **algorithm choice** — they
only protect data structures TBB does not already make concurrent for you.

---

## Summary

- **`spin_mutex`**: busy-wait, lowest overhead **uncontended**, wastes cycles
  and risks **starvation** under contention — **only for tiny** sections.
- **`queuing_mutex`**: **FIFO-fair** spinning queue — better when many threads
  fight over one lock for **short** but non-trivial sections.
- Both use **`scoped_lock`**; choose by **hold time × contention**, not habit.
- Prefer **reductions / atomics / concurrent containers** before any mutex.

Next: [4.3 — Reader-writer locks](03-reader-writer-locks.md)
