# 2.2 — task_arena

A **`task_arena`** is a bounded pool of **slots** (`max_concurrency`) that worker threads
join to execute tasks. The global scheduler still owns OS threads; an arena **limits how
many** of those threads participate in *your* parallel work and **isolates** competing
subsystems from each other's task queues.

```
   global worker pool (≈ one thread per core)
        │
        ├── default implicit arena  ← parallel_for with no arena wrapper
        │
        ├── task_arena(2)  ── execute(batch_job)     max 2 workers here
        │
        └── task_arena(4)  ── execute(interactive)   max 4 workers here
```

---

## 2.2.1 Slots, workers, and `max_concurrency`

Construct an arena with an optional concurrency cap:

```cpp
tbb::task_arena arena(4);              // at most 4 workers in this arena
tbb::task_arena arena;                 // default: info::default_concurrency()
int slots = arena.max_concurrency();
```

> **The API ▸**
> ```cpp
> // <oneapi/tbb/task_arena.h>
> class task_arena {
> public:
>     explicit task_arena(int max_concurrency = automatic,
>                         unsigned reserved_for_masters = 1);
>     template<typename F> void execute(F&& f);
>     template<typename F> void enqueue(F&& f);
>     int max_concurrency() const;
> };
> namespace this_task_arena {
>     int max_concurrency();
>     int current_thread_index();
> }
> ```
> **`execute(f)`** runs `f` on a worker **attached to this arena**; parallel algorithms
> called inside `f` inherit the arena's concurrency limit. **`enqueue(f)`** schedules work
> asynchronously (may run after `execute` returns unless you synchronize).

![task_arena: bounded slots isolate workloads on the shared worker pool](figures/task-arena.svg)

---

## 2.2.2 `arena.execute([]{ ... })`

All TBB parallel work inside the lambda uses **this arena's slot count**:

```cpp
tbb::task_arena compute_arena(8);

compute_arena.execute([&] {
    tbb::parallel_for(0, n, [&](int i) { work(i); });
    // at most 8 workers participate, even on a 32-core machine
});
```

Nested `parallel_for` inside the same `execute` shares the arena — still capped at 8, no
oversubscription beyond the arena limit.

> **Under the hood ▸** `execute` binds the calling thread to the arena for the duration of
> `f`. Tasks spawned inside inherit that binding. Workers from the global pool **join**
> the arena temporarily to steal arena-local tasks.

---

## 2.2.3 Isolating parallelism

Run independent workloads in separate arenas so a batch job cannot steal all cores from
latency-sensitive work:

```cpp
tbb::task_arena batch(6);
tbb::task_arena interactive(2);

std::thread batch_thread([&] {
    batch.execute([] { run_nightly_etl(); });
});

interactive.execute([] { serve_user_requests(); });
batch_thread.join();
```

Each arena gets its own task queues and slot budget. Total slots can exceed core count if
arenas run concurrently on different OS threads — **you** are responsible for not
oversubscribing the machine across arenas.

**Trade-offs ▸** Isolation improves tail latency; sum of arena limits > core count →
context-switch thrash.

---

## 2.2.4 Blocking and latency-sensitive work

**Never block** a worker on I/O, locks held for milliseconds, or `sleep` inside the
**default** arena — you starve unrelated `parallel_for` calls app-wide.

Patterns:

```
   ✓  Run blocking server loop on std::thread, offload CPU work via arena.execute
   ✓  task_arena(1).execute for serial fallback testing
   ✓  Dedicated small arena for latency work; large arena for batch
   ✗  parallel_for body that waits on network inside default arena
```

See Part 0.1 — I/O-bound waiting belongs in async I/O, not TBB workers.

---

## 2.2.5 `this_task_arena`

Query the **current execution context** from inside parallel code:

```cpp
tbb::parallel_for(0, n, [&](int) {
    int cap = tbb::this_task_arena::max_concurrency();
    int idx = tbb::this_task_arena::current_thread_index();
    // idx in [0, cap) for workers in this arena; -1 if not on a worker
});
```

Use **`max_concurrency()`** to size per-thread buffers (Part 5.3 ETS) or shard work.
**`current_thread_index()`** identifies which slot a worker occupies — useful for
NUMA-first pinning experiments (advanced).

---

## 2.2.6 Interaction with the global pool

```
   one OS thread pool (lazy-init on first TBB call, Part 2.3)
        │
        workers migrate between arenas as execute/enqueue demands
        │
   default implicit arena  ≈  all slots ≈ info::default_concurrency()
   explicit arena(N)       ≈  at most N slots for nested parallel work
```

`global_control::max_allowed_parallelism` caps the **entire process** (Part 7.1); arena
caps apply **per execute scope**. Effective workers = min(global cap, arena cap, hardware).

**Tuning ▸** Start with the default arena. Introduce explicit arenas when profiling shows
interference (batch vs interactive, nested OpenMP + TBB, blocking callbacks).

---

## 2.2.7 Example: capped parallel section

```cpp
// g++ -std=c++17 -O2 task_arena_demo.cpp -ltbb
#include <oneapi/tbb/task_arena.h>
#include <oneapi/tbb/parallel_for.h>
#include <oneapi/tbb/info.h>
#include <atomic>
#include <cstdio>
#include <vector>

int main() {
    const int n = 10'000'000;
    std::vector<int> data(n, 1);

    std::printf("default_concurrency = %d\n", tbb::info::default_concurrency());

    tbb::task_arena limited(2);
    std::atomic<int> peak{0};

    limited.execute([&] {
        tbb::parallel_for(0, n, [&](int i) {
            int idx = tbb::this_task_arena::current_thread_index();
            if (idx >= 0) {
                int seen = peak.load(std::memory_order_relaxed);
                while (idx + 1 > seen &&
                       !peak.compare_exchange_weak(seen, idx + 1)) {}
            }
            data[i] *= 2;
        });
    });

    std::printf("arena max_concurrency = %d  peak worker index+1 ≈ %d\n",
                limited.max_concurrency(), peak.load());
    return 0;
}
```

---

## Summary

- **`task_arena`** bounds **max_concurrency** (slots) for parallel work inside it.
- **`execute`** runs code on the arena; nested algorithms inherit the cap.
- Use arenas to **isolate** subsystems and keep **blocking work** off shared workers.
- **`this_task_arena`** queries current arena limits and worker index.
- Arenas sit on top of the **global worker pool**; combine with `global_control` for
  process-wide caps (Part 7.1).

Next: [2.3 — The task scheduler & control](03-task-scheduler-and-control.md)
