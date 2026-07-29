# 4.1 — Mutexes: the TBB Family

When concurrent containers and parallel algorithms (Parts 1–3) cannot express
your sharing pattern, you reach for **mutexes**. oneTBB ships a **family** of
mutex types tuned for different contention profiles — all sharing the same
**`scoped_lock` RAII idiom**: construct the lock to acquire, destroy to release,
exception-safe by default.

The engineering default: **prefer concurrent containers, atomics, or reductions
over explicit locks.** Mutexes serialize; serialization caps scaling (Part 7.4,
Amdahl). Use them for **invariants on non-container state** — log buffers,
connection pools, legacy APIs.

---

## 4.1.1 The scoped_lock pattern (every TBB mutex)

```
   {                                         
       tbb::spin_mutex::scoped_lock lock(m);  ──▶  acquire(m)
       shared_counter += 1;                   ──▶  critical section
   }                                          ──▶  ~scoped_lock → release(m)
```

> **The API ▸**
> ```cpp
> // Every TBB mutex M has a nested type:
> M::scoped_lock lock(mutex_object);           // exclusive
> M::scoped_lock lock(rw_mutex, /*write=*/false);  // RW: read mode
> ```
> Headers: `<oneapi/tbb/mutex.h>`, `spin_mutex.h`, `queuing_mutex.h`,
> `spin_rw_mutex.h`, `queuing_rw_mutex.h`, `null_mutex.h`.
> **Never** call `lock()`/`unlock()` manually unless you have an exceptional
> reason — scoped locks survive exceptions and early `return`.

Same pattern everywhere — learn once, apply to all mutex types.

---

## 4.1.2 The family at a glance

![TBB mutex types compared by blocking behavior and fairness](figures/mutex-types.svg)

```
   contention low, critical section TINY     →  spin_mutex
   contention low–med, general purpose       →  mutex (OS-aware)
   contention high, fairness required        →  queuing_mutex
   many readers, rare writer                 →  spin_rw_mutex / queuing_rw_mutex
   single-threaded template code             →  null_mutex (zero cost)
```

| Mutex | Waits by | Fair? | Best for |
|-------|----------|-------|----------|
| `spin_mutex` | busy-spin | ✗ (starvation possible) | sub-µs critical sections |
| `mutex` | OS block when contended | roughly fair | default exclusive lock |
| `queuing_mutex` | spin + FIFO queue | ✓ FIFO | sustained contention, latency fairness |
| `spin_rw_mutex` | spin; shared/exclusive | ✗ | read-heavy, short sections |
| `queuing_rw_mutex` | queue; shared/exclusive | ✓ | read-heavy + fairness |
| `null_mutex` | no-op | n/a | single-threaded instantiations |

Deep dives: [spin_mutex & queuing_mutex](02-spin-and-queuing-mutex.md),
[Reader-writer locks](03-reader-writer-locks.md).

> **Under the hood ▸** `spin_mutex` never enters the kernel while waiting —
> it burns cycles. `mutex` spins briefly then **blocks** the thread (futex/
> semaphore), freeing the core. `queuing_mutex` links waiters in a **FIFO
> queue** so the next owner is deterministic. Pick based on **hold time** and
> **contention**, not folklore.

---

## 4.1.3 How to choose

```
   critical section < ~1 µs, low contention?
        yes → spin_mutex
        no  ↓
   mostly reads, rare writes, section non-trivial?
        yes → spin_rw_mutex or queuing_rw_mutex (Part 4.3)
        no  ↓
   many threads hammering one lock for milliseconds?
        yes → queuing_mutex (fairness + predictable latency)
        no  → mutex (good default)
```

**Trade-offs ▸**
- ✓ Correctness for arbitrary shared mutable state; uniform scoped_lock API.
- ✗ Serializes threads; lock order bugs; priority inversion (OS mutex);
  cache-line bouncing on hot locks ([Part 5.2](../05-memory/02-cache-alignment-false-sharing.md)).

> **Pitfall ▸** Wrapping a **`concurrent_vector::push_back`** in your own
> mutex defeats the container's purpose. Wrapping **`parallel_reduce`** body
> in a global lock makes runtime parallelism pointless. Ask: can this be a
> **reduction**, **concurrent container**, or **`std::atomic`** (Part 4.4)?

---

## 4.1.4 null_mutex: zero-cost placeholder

> **The API ▸**
> ```cpp
> #include <oneapi/tbb/null_mutex.h>
> tbb::null_mutex m;
> tbb::null_mutex::scoped_lock lock(m);  // compiles; does nothing
> ```
> Use in **templates** parameterized on a Mutex type: instantiate with
> `null_mutex` single-threaded, `mutex` multi-threaded — no `#ifdef` branches
> in algorithm code.

---

## 4.1.5 Example: when a mutex is actually appropriate

Protecting a non-TBB resource (here, a serial log stream) while compute stays
parallel:

```cpp
// g++ -std=c++17 -O2 mutex_demo.cpp -ltbb
#include <oneapi/tbb/mutex.h>
#include <oneapi/tbb/parallel_for.h>
#include <oneapi/tbb/blocked_range.h>
#include <cstdio>
#include <vector>

int main() {
    tbb::mutex log_mtx;
    std::vector<int> results(1'000'000);

    tbb::parallel_for(
        tbb::blocked_range<size_t>(0, results.size()),
        [&](const tbb::blocked_range<size_t>& r) {
            for (size_t i = r.begin(); i != r.end(); ++i) {
                results[i] = static_cast<int>(i * i);   // parallel — no lock
                if (i % 250'000 == 0) {
                    tbb::mutex::scoped_lock lock(log_mtx);   // rare serial I/O
                    std::printf("progress at %zu\n", i);
                }
            }
        });
    return 0;
}
```

Keep critical sections **short** and **cold** (not in inner loops). If the
locked region is only incrementing an integer, use [`std::atomic`](04-atomics.md).

---

## Summary

- Every TBB mutex uses **`Mutex::scoped_lock`** for RAII acquire/release.
- **`spin_mutex`** (tiny sections), **`mutex`** (default), **`queuing_mutex`**
  (fair under contention), **RW variants** (read-heavy), **`null_mutex`**
  (template stub).
- **Prefer concurrent containers and reductions** over wrapping parallel work
  in locks; mutexes for true shared non-composable state.
- False sharing on lock-protected counters still hurts — see
  [Part 5.2](../05-memory/02-cache-alignment-false-sharing.md).

Next: [4.2 — spin_mutex & queuing_mutex](02-spin-and-queuing-mutex.md)
