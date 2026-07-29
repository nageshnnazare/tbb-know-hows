# The Intel oneTBB Mastery Guide

> A single, deep, diagram-driven reference for how Intel's oneAPI Threading
> Building Blocks (oneTBB) actually work: from the work-stealing scheduler and
> the task model, through the parallel algorithms, concurrent containers, and
> synchronization primitives, to the Flow Graph and the performance engineering
> (grain size, false sharing, scalable allocation) that decides whether your
> parallel code scales or stalls.
>
> Written for **C++ engineers who want expert-level mechanical detail**, not a
> tour of function signatures. Every construct is grounded in *what the runtime
> does underneath* — the deques, the split trees, the cache lines — with
> diagrams, compilable C++17, and the trade-offs that matter in production.

---

## Who this is for

You can already write modern C++. You may have called `std::thread`, sprinkled
a `#pragma omp parallel for`, or used `std::async`. But you want to *truly*
understand:

- Why TBB uses **tasks** instead of threads, and why "1 task = 1 thread" is the
  classic beginner mistake.
- How the **work-stealing scheduler** balances load with almost no contention —
  and why it steals the *oldest* task, not the newest.
- What **grain size** really controls, and why the wrong value makes parallel
  code slower than serial.
- Why `parallel_reduce` results aren't bit-identical across runs (associativity
  vs floating point).
- How `concurrent_vector` keeps `&v[i]` valid across growth when `std::vector`
  can't.
- What an **accessor** locks in `concurrent_hash_map`, and how holding two
  deadlocks you.
- Why **false sharing** can make a 16-core run *slower* than a 1-core run, and
  how `cache_aligned_allocator` fixes it.
- When to reach for `spin_mutex` vs `mutex` vs `queuing_mutex` vs an `atomic`.
- How the **Flow Graph** turns an irregular dependency DAG into scheduled work.
- Why Amdahl's law caps your speedup, and how to find the serial fraction.

If you finish this guide, you will be able to read the oneTBB headers, reason
about a `perf`/VTune profile of a parallel program, and design parallelism that
actually scales on real hardware.

---

## The 30,000-foot map

```
   you express WHAT is parallel                 the runtime decides WHEN & WHERE
   ─────────────────────────────                ────────────────────────────────

   parallel_for / reduce / scan  ┐
   parallel_pipeline             │   split into
   task_group / flow graph       ├──▶  TASKS  ──┐
   concurrent containers         ┘               │
                                                 ▼
                        ┌───────────────────────────────────────────┐
                        │        WORK-STEALING SCHEDULER              │
                        │  each worker owns a deque of ready tasks    │
                        │  local LIFO (cache-hot) · steal oldest FIFO │
                        └───────────────────────────────────────────┘
                                                 │  one worker per core
                                                 ▼
                        ┌───────────────────────────────────────────┐
                        │  OS threads pinned to hardware cores        │
                        │  + tbbmalloc: per-thread scalable heaps     │
                        └───────────────────────────────────────────┘
```

Each box is a chapter this guide dissects. Each arrow is a mechanism with a
cost, a tuning knob, and a failure mode.

---

## How to read this guide

The parts are ordered as a **learning path** from the scheduler up to production
performance tuning. If you already know the task model, jump to Part 1
(algorithms), Part 3 (containers), or Part 7 (performance) — the
engineering-heavy heart of the guide.

Every chapter has:

- **Concept** sections with hand-drawn diagrams.
- **The API ▸** call-outs: exact signature, header, and semantics.
- **Under the hood ▸** boxes: what the scheduler/allocator does underneath.
- **Example ▸** blocks: compilable, correct C++17 (`-std=c++17 -ltbb`).
- **Trade-offs ▸** and **Pitfall ▸**: real scaling failures explained by the
  mechanics.
- **Tuning ▸**: grain size, partitioners, alignment, and measurement.

---

## Table of contents

### Part 0 — Foundations (`00-foundations/`)
1. [What is TBB? The big picture](00-foundations/01-what-is-tbb.md)
2. [Tasks vs threads](00-foundations/02-tasks-vs-threads.md)
3. [The work-stealing scheduler](00-foundations/03-work-stealing-scheduler.md)
4. [Ranges, splitting & grain size](00-foundations/04-ranges-and-grain-size.md)
5. [Partitioners](00-foundations/05-partitioners.md)

### Part 1 — Parallel algorithms (`01-parallel-algorithms/`)
1. [parallel_for](01-parallel-algorithms/01-parallel-for.md)
2. [parallel_reduce](01-parallel-algorithms/02-parallel-reduce.md)
3. [parallel_scan](01-parallel-algorithms/03-parallel-scan.md)
4. [parallel_sort](01-parallel-algorithms/04-parallel-sort.md)
5. [parallel_invoke](01-parallel-algorithms/05-parallel-invoke.md)
6. [parallel_pipeline](01-parallel-algorithms/06-parallel-pipeline.md)

### Part 2 — Task-based programming (`02-task-programming/`)
1. [task_group](02-task-programming/01-task-group.md)
2. [task_arena](02-task-programming/02-task-arena.md)
3. [The task scheduler & control](02-task-programming/03-task-scheduler-and-control.md)
4. [Continuations & dependencies](02-task-programming/04-continuations-and-dependencies.md)

### Part 3 — Concurrent containers (`03-concurrent-containers/`)
1. [concurrent_vector](03-concurrent-containers/01-concurrent-vector.md)
2. [concurrent_queue](03-concurrent-containers/02-concurrent-queue.md)
3. [concurrent_hash_map](03-concurrent-containers/03-concurrent-hash-map.md)
4. [concurrent_unordered_map](03-concurrent-containers/04-concurrent-unordered-map.md)
5. [Bounded & priority queues](03-concurrent-containers/05-bounded-and-priority-queues.md)

### Part 4 — Synchronization (`04-synchronization/`)
1. [Mutexes: the TBB family](04-synchronization/01-mutexes.md)
2. [spin_mutex & queuing_mutex](04-synchronization/02-spin-and-queuing-mutex.md)
3. [Reader-writer locks](04-synchronization/03-reader-writer-locks.md)
4. [Atomics](04-synchronization/04-atomics.md)

### Part 5 — Memory & thread-local (`05-memory/`)
1. [The scalable allocator](05-memory/01-scalable-allocator.md)
2. [Cache alignment & false sharing](05-memory/02-cache-alignment-false-sharing.md)
3. [Thread-local storage (enumerable_thread_specific)](05-memory/03-thread-local-storage.md)

### Part 6 — Flow Graph (`06-flow-graph/`)
1. [Flow Graph fundamentals](06-flow-graph/01-flow-graph-intro.md)
2. [Function & buffer nodes](06-flow-graph/02-function-and-buffer-nodes.md)
3. [Join & split nodes](06-flow-graph/03-join-and-split-nodes.md)
4. [Priorities & limiters](06-flow-graph/04-priorities-and-limiters.md)

### Part 7 — Advanced & performance (`07-advanced-performance/`)
1. [global_control](07-advanced-performance/01-global-control.md)
2. [task_scheduler_observer](07-advanced-performance/02-task-scheduler-observer.md)
3. [Deterministic reduction](07-advanced-performance/03-deterministic-reduce.md)
4. [Performance tuning & Amdahl's law](07-advanced-performance/04-performance-tuning.md)
5. [Common pitfalls](07-advanced-performance/05-common-pitfalls.md)

### Reference (`99-reference/`)
- [API cheat sheet](99-reference/api-cheatsheet.md)
- [Glossary](99-reference/glossary.md)

### Runnable examples (`examples/`)
35 compilable `.cpp` programs, one per major topic. Build them all with:

```bash
cd examples && make        # needs a oneTBB install (libtbb-dev / brew install tbb)
```

---

## Conventions used in this guide

| Notation / call-out | Meaning                                                    |
|---------------------|------------------------------------------------------------|
| **The API ▸**       | Exact signature, header, and semantics                     |
| **Under the hood ▸**| What the scheduler / allocator does underneath             |
| **Example ▸**       | Compilable, correct C++17 (`-std=c++17 -ltbb`)             |
| **Trade-offs ▸**    | Advantages vs disadvantages of a construct                 |
| **Pitfall ▸**       | A common mistake explained mechanically                    |
| **Tuning ▸**        | A grain-size / partitioner / alignment lever               |
| task                | A lightweight unit of work (NOT an OS thread)              |
| worker              | An OS thread the scheduler runs tasks on (≈ one per core) |
| grain size          | The smallest subrange the scheduler will not split further |

Namespace note: this guide uses the modern **oneTBB** API under `oneapi::tbb`
(aliased as `tbb`), e.g. `tbb::parallel_for`, `tbb::concurrent_vector`. The
legacy `task`-derived API removed in oneTBB is called out where it matters.

---

## The one idea that never changes (read this first)

TBB is built on a single trade: **you describe the available parallelism; the
runtime decides how to realize it.**

```cpp
#include <oneapi/tbb/parallel_for.h>
#include <oneapi/tbb/blocked_range.h>

tbb::parallel_for(
    tbb::blocked_range<size_t>(0, n),          // WHAT can run in parallel
    [&](const tbb::blocked_range<size_t>& r) { // the runtime picks WHEN/WHERE
        for (size_t i = r.begin(); i != r.end(); ++i)
            a[i] = f(a[i]);
    });
```

You never create threads, never assign work to a core, never write a steal
loop. You state that the iterations are independent; the scheduler splits the
range into tasks, spreads them across workers, and balances the load by
stealing. Get the *decomposition* and the *grain size* right and the rest is
automatic. We belabor those two on purpose.

Let's begin. → [Part 0.1: What is TBB?](00-foundations/01-what-is-tbb.md)
