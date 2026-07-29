# 7.2 — task_scheduler_observer

The work-stealing scheduler (Part 0.3) creates and reuses a fixed pool of worker
threads. Sometimes you need code to run **when a thread enters or leaves** that
pool — to pin a worker to a core, initialize thread-local state, or count active
workers. **`tbb::task_scheduler_observer`** is the hook the runtime calls at those
boundaries.

---

## 7.2.1 Entry and exit callbacks

Subclass `task_scheduler_observer` and override two virtual methods. The runtime
invokes them whenever a thread **joins** or **leaves** participation in an arena's
scheduler loop — not on every task, but on **worker lifecycle** events within that
arena.

```
   worker thread timeline
   ──────────────────────
        on_scheduler_entry(is_worker=true)
              │
              ├─ execute tasks … steal … execute …
              │
        on_scheduler_exit(is_worker=true)

   calling thread (master) may also trigger entry/exit when it
   participates in parallel algorithms (is_worker=false).
```

> **The API ▸**
> ```cpp
> #include <oneapi/tbb/task_scheduler_observer.h>
>
> class task_scheduler_observer {
> public:
>     explicit task_scheduler_observer(tbb::task_arena& a);
>     task_scheduler_observer();  // observes default/current arena context
>     void observe(bool enable = true);  // activate / deactivate callbacks
> protected:
>     virtual void on_scheduler_entry(bool is_worker);
>     virtual void on_scheduler_exit(bool is_worker);
> };
> ```
> **`is_worker`:** `true` for TBB pool workers; `false` for the **master** thread
> (the thread that entered the arena to run parallel work, including the caller of
> `parallel_for`). Both paths may call your hooks — filter on `is_worker` if you
> only care about workers.

> **Under the hood ▸** Callbacks run **on the thread entering/exiting** — not on a
> central admin thread. Keep them fast: no blocking I/O, no heavy allocation. They
> fire under scheduler locks in some builds; lengthy work stalls scheduling.

---

## 7.2.2 Activating with observe(true)

Observers are **inactive until enabled**. The usual pattern is to call
`observe(true)` in the subclass constructor:

```cpp
class MyObserver : public tbb::task_scheduler_observer {
public:
    MyObserver() { observe(true); }  // start receiving callbacks immediately
    // ...
};
```

Call `observe(false)` to pause callbacks without destroying the object — useful
for toggling instrumentation in debug builds. Destroying the observer while
`observe(true)` automatically deregisters it.

> **Pitfall ▸** Forgetting `observe(true)` gives you a silent no-op observer.
> If your pinning or counters never change, check activation first.

---

## 7.2.3 Use cases

| Use case | What to do in `on_scheduler_entry` |
|----------|-------------------------------------|
| **CPU affinity / pinning** | Bind the worker to a core via `sched_setaffinity` / `SetThreadAffinityMask` |
| **Per-thread library init** | Open a DB connection, TLS context, or GPU device handle |
| **NUMA binding** | `numa_run_on_node()` / `mbind` for socket-local memory |
| **RNG seeding** | Seed a thread-local generator in `enumerable_thread_specific` (Part 5.3) |
| **Monitoring** | Increment active-worker count; record timestamps for VTune correlation |
| **Custom naming** | Set the OS thread name for debuggers (`pthread_setname_np`) |

Teardown belongs in `on_scheduler_exit`: flush buffers, release handles, unbind
affinity. Symmetric entry/exit keeps per-thread state consistent even when workers
are parked and woken across parallel phases.

---

## 7.2.4 Arena-specific observers

Pass a **`task_arena&`** to the constructor to observe **only that arena's**
workers. The default constructor attaches to the **current arena context** when
constructed — for global observation of the default arena, construct early (before
parallel work) on the thread that will run TBB.

```
   default arena                    task_arena(4)  "slow lane"
   ┌─────────────────┐              ┌─────────────────┐
   │ observer A      │              │ observer B      │
   │ (all default    │              │ (only these 4   │
   │  pool workers)  │              │  workers)       │
   └─────────────────┘              └─────────────────┘
```

This pairs with Part 2.2: pin the "compute-heavy" arena to performance cores and
leave the default pool unpinned, or attach different NUMA policies per arena.

**Trade-offs ▸** Observers see **scheduler** entry/exit, not individual tasks.
For per-task tracing, wrap bodies or use Intel VTune's TBB annotations. Observers
add a small tax on every worker park/wake — disable in release hot paths.

---

## 7.2.5 Example: thread counter and optional pinning

The example below counts how many workers are active concurrently and demonstrates
the pinning pattern (Linux `sched_setaffinity`; guarded so it compiles everywhere).

```cpp
// g++ -std=c++17 -O2 observer_demo.cpp -ltbb -lpthread
#include <oneapi/tbb/task_scheduler_observer.h>
#include <oneapi/tbb/parallel_for.h>
#include <oneapi/tbb/blocked_range.h>
#include <oneapi/tbb/task_arena.h>
#include <atomic>
#include <cstdio>
#include <mutex>
#include <thread>

#if defined(__linux__)
#include <sched.h>
#endif

class ThreadCounter : public tbb::task_scheduler_observer {
    std::atomic<int> active_{0};
    std::atomic<int> max_active_{0};
    std::atomic<int> total_entries_{0};

public:
    ThreadCounter() { observe(true); }

    void on_scheduler_entry(bool is_worker) override {
        if (!is_worker) return;
        const int now = ++active_;
        total_entries_.fetch_add(1, std::memory_order_relaxed);
        int expected = max_active_.load(std::memory_order_relaxed);
        while (now > expected &&
               !max_active_.compare_exchange_weak(expected, now,
                   std::memory_order_relaxed)) {}
    }

    void on_scheduler_exit(bool is_worker) override {
        if (!is_worker) return;
        --active_;
    }

    void report() const {
        std::printf("max concurrent workers: %d  total entries: %d\n",
            max_active_.load(), total_entries_.load());
    }
};

class PinningObserver : public tbb::task_scheduler_observer {
    std::atomic<int> next_core_{0};

public:
    explicit PinningObserver(tbb::task_arena& arena) : tbb::task_scheduler_observer(arena) {
        observe(true);
    }

    void on_scheduler_entry(bool is_worker) override {
        if (!is_worker) return;
#if defined(__linux__)
        const int n = static_cast<int>(std::thread::hardware_concurrency());
        if (n <= 0) return;
        cpu_set_t cpuset;
        CPU_ZERO(&cpuset);
        CPU_SET(static_cast<unsigned>(next_core_.fetch_add(1) % n), &cpuset);
        sched_setaffinity(0, sizeof(cpuset), &cpuset);
#endif
    }
};

int main() {
    ThreadCounter counter;

    tbb::parallel_for(
        tbb::blocked_range<int>(0, 10'000'000, 10'000),
        [](const tbb::blocked_range<int>& r) {
            double s = 0;
            for (int i = r.begin(); i != r.end(); ++i)
                s += std::sqrt(static_cast<double>(i));
            (void)s;
        });

    counter.report();

    tbb::task_arena pinned_arena(4);
    PinningObserver pin_obs(pinned_arena);

    pinned_arena.execute([] {
        tbb::parallel_for(0, 1'000'000, [](int i) { (void)i; });
    });

    return 0;
}
```

On Linux with affinity enabled, each worker in `pinned_arena` receives a rotating
core assignment at first entry. The counter shows how many workers ran simultaneously
— compare against `global_control` caps from Part 7.1.

> **Tuning ▸** Pinning can **hurt** performance if it fights the OS scheduler or
> spreads workers across NUMA nodes incorrectly. Measure with and without pinning
> (Part 7.4). Combine with `enumerable_thread_specific` (Part 5.3) for per-worker
> data instead of global atomics when counters become hot.

---

## Summary

- **`task_scheduler_observer`** hooks **worker entry/exit** in an arena — not
  per-task execution.
- Override **`on_scheduler_entry`** / **`on_scheduler_exit`**; filter on
  **`is_worker`** if you only want pool workers.
- Call **`observe(true)`** to activate; pass a **`task_arena&`** for arena-local
  observation.
- Use for **affinity, NUMA binding, per-thread init/teardown, and monitoring** —
  keep callbacks short and non-blocking.
- Pairs with **`task_arena`** (Part 2.2) for isolated pools and **`global_control`**
  (Part 7.1) for worker-count caps.

Next: [7.3 — Deterministic reduction](03-deterministic-reduce.md)
