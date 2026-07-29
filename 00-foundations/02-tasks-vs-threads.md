# 0.2 — Tasks vs Threads

Part 0.1 introduced TBB as a **task scheduler** layered over OS threads. This chapter
makes that distinction mechanical: what a *task* actually is, why it is orders of
magnitude cheaper than a thread, and why treating them as interchangeable is the
classic scaling failure.

---

## 0.2.1 Two different animals

An **OS thread** is a kernel-managed execution context: its own stack (typically
1–8 MiB), register state, scheduling slot, and bookkeeping in the kernel's run
queue. Creating or destroying one costs **microseconds** and involves a syscall.

A **TBB task** is a small **function object** — a callable plus captured state —
that the runtime queues for a worker to execute. Spawning a task costs **tens of
nanoseconds**: allocate a small task object, push a pointer onto a deque, maybe
wake an idle worker. No syscall, no new stack, no new kernel thread.

![Tasks are lightweight work units scheduled onto a fixed pool of OS worker threads](figures/task-vs-thread.svg)

```
   OS THREAD                          TBB TASK
   ─────────                          ────────
   kernel resource                    user-space object (~few hundred bytes)
   own stack (MiB)                    runs on worker's existing stack
   create ≈ 1–100 µs                  spawn ≈ 20–100 ns
   context switch ≈ 1–10 µs           handoff via deque push/pop
   you manage lifetime                scheduler recycles task objects
```

> **The key idea ▸** You write *tasks* (units of work). TBB runs them on a fixed
> pool of *workers* (OS threads). The mapping from tasks → workers is dynamic,
> many-to-few, and changes every microsecond. You never choose it.

Part 0.3 explains how workers pick tasks from their deques and steal from each
other. For now: **one worker ≈ one hardware core**, and **many tasks** time-slice
across those workers.

---

## 0.2.2 The worker pool

When your program first calls into TBB, the runtime creates a **thread pool**
(inside an *arena* — see Part 2.2) sized to the machine's parallelism:

```
   hardware_concurrency()  →  typically = physical cores (or logical if HT exposed)
                                    │
                                    ▼
   ┌─────────┬─────────┬─────────┬─────────┐
   │ worker 0│ worker 1│ worker 2│ worker …│   ← OS threads, long-lived
   └────┬────┴────┬────┴────┬────┴────┬────┘
        │         │         │         │
     deque 0   deque 1   deque 2   deque …   ← each worker owns a task deque
```

> **Under the hood ▸** The default worker count comes from
> `std::thread::hardware_concurrency()` unless overridden by
> `tbb::global_control` (Part 7.1). Workers are created once and reused for the
> process lifetime. Tasks are **not** pinned to a worker across calls — only
> *affinity_partitioner* (Part 0.5) nudges the mapping toward repeatability.

Each worker loop is conceptually:

```
   while (pool active) {
       if (my deque has work)  → pop task, execute it
       else if (can steal)     → steal from another worker's deque
       else                     → park until woken
   }
```

Thousands of tasks may execute on a single worker between two lines of your
`main()`. From the hardware's perspective there are ~N threads; from yours there
are ~M tasks where M ≫ N.

---

## 0.2.3 Why "1 task = 1 thread" fails

The most common mental model error when coming from `std::thread`:

```
   ✗ WRONG:  "I have 1 million iterations → spawn 1 million threads"
   ✓ RIGHT:  "I have 1 million iterations → split into ~N chunks → ~N tasks at a time"
```

If you map work 1:1 to threads:

| Effect | Mechanism |
|--------|-----------|
| Memory explosion | 1M threads × ~2 MiB stack ≈ terabytes of virtual address space |
| Scheduler meltdown | kernel thrashes between thousands of runnable threads |
| Cache destruction | constant context switches evict hot data |
| No load balance | static assignment; stragglers stall the join |

TBB never does this. A `parallel_for` over 10⁷ elements might create **hundreds**
of tasks over the run, but only **P tasks execute concurrently** where P ≈ core
count. Finished tasks are destroyed or recycled; new splits fill idle workers.

**Trade-offs ▸** Task granularity is the knob you control (via **grain size**,
Part 0.4). Too many tiny tasks → scheduling overhead dominates. Too few large
tasks → cores idle waiting for stragglers. The scheduler assumes you will get
this roughly right; it cannot fix a body that does nanoseconds of work per
iteration with grain size 1.

---

## 0.2.4 Oversubscription

**Oversubscription** means having **more runnable OS threads than physical cores**.
Each extra thread adds context-switch cost without adding throughput.

```
   16 cores, 16 workers  →  ✓ cores stay busy, minimal switching
   16 cores, 200 threads →  ✗ 184 threads wait; kernel time wasted
```

Hand-rolled parallelism oversubscribes easily:

```
   outer parallel_for  → 16 threads
       inner parallel_for  → × 16 each  →  256 threads on 16 cores  ✗
```

TBB composes because **nested** `parallel_for` calls enqueue tasks on the **same**
scheduler. The inner loop does not spawn 16 new threads; it adds tasks to deques
that existing workers steal. Total concurrency stays ≈ core count (Part 0.1).

> **Pitfall ▸** Mixing TBB with raw `std::thread` or OpenMP *inside* a TBB body
> bypasses this guarantee. A `parallel_for` body that launches 8 `std::thread`s
> can still oversubscribe. Keep parallelism at the TBB layer; use
> `task_group` (Part 2.1) for unstructured fork-join inside TBB.

---

## 0.2.5 Blocking a worker starves the pool

A task runs **on** a worker's stack. If the task **blocks** — `mutex.lock()` on
contention, `condition_variable.wait()`, `read()` on a socket, `sleep()` — that
worker stops dequeuing and stealing work. With P workers and K blocked tasks, you
effectively have P − K cores doing useful work.

```
   8 workers, 3 blocked on I/O  →  at most 5 cores productive
                                  →  scheduler may spawn extra workers (arena
                                     policy) but cannot create throughput from wait
```

This is why TBB is a poor fit for I/O-bound workloads (Part 0.1). For the
occasional blocking call inside CPU work, isolate it with **`task_arena`**
(Part 2.2) or run blocking sections outside the parallel region.

> **Under the hood ▸** Modern oneTBB arenas can **market** tasks across arenas and
> temporarily increase the worker count when starvation is detected, but blocking
> remains antithetical to work-stealing efficiency. Design bodies to be
> **compute-bound** and **non-blocking**.

---

## 0.2.6 Composability in practice

Because every TBB algorithm lowers to the same task pool, parallelism **nests
safely**:

```cpp
// g++ -std=c++17 -O2 nested_tasks.cpp -ltbb -o nested_tasks
#include <oneapi/tbb/parallel_for.h>
#include <oneapi/tbb/blocked_range.h>
#include <oneapi/tbb/blocked_range2d.h>
#include <vector>
#include <cstdio>

int main() {
    const int N = 512;
    std::vector<double> matrix(N * N, 1.0);

    // Outer: rows. Inner: columns. Both become tasks on one scheduler.
    tbb::parallel_for(
        tbb::blocked_range<int>(0, N),
        [&](const tbb::blocked_range<int>& rows) {
            tbb::parallel_for(
                tbb::blocked_range<int>(0, N),
                [&](const tbb::blocked_range<int>& cols) {
                    for (int i = rows.begin(); i != rows.end(); ++i)
                        for (int j = cols.begin(); j != cols.end(); ++j)
                            matrix[i * N + j] *= 2.0;
                });
        });

    std::printf("matrix[0]=%g\n", matrix.front());
    return 0;
}
```

Neither loop creates new OS threads. The outer split enqueues tasks; while a
worker executes an outer subrange, it may spawn inner splits as **child tasks**
that siblings steal. Total thread count stays flat; total **task** count grows
with nesting depth and grain size (Part 0.4).

The same composability applies across library boundaries: your `parallel_reduce`
can call a third-party function that internally uses `parallel_for`, and both
share one pool — impossible to coordinate this cleanly with ad-hoc threads.

---

## 0.2.7 Tasks vs threads — side by side

| | OS thread (`std::thread`) | TBB task |
|---|---------------------------|----------|
| Created by | you, explicitly | runtime, from splits / algorithms |
| Cost to create | ~µs (syscall) | ~ tens of ns |
| Stack | dedicated, large | borrow worker's stack |
| Count in a healthy program | ≈ core count | thousands over time, P concurrent |
| Load balancing | your problem | work-stealing (Part 0.3) |
| Nested parallelism | multiplies threads | enqueues more tasks |
| Lifetime | join or detach | ends when body returns |

> **The API ▸** You rarely construct tasks directly in modern oneTBB. Instead you
> call high-level entry points that create them for you:
>
> ```cpp
> #include <oneapi/tbb/parallel_for.h>
> tbb::parallel_for(range, body);           // algorithms → tasks
> #include <oneapi/tbb/task_group.h>
> tbb::task_group g; g.run([]{ ... });       // explicit unstructured tasks
> ```
>
> Headers: `<oneapi/tbb/parallel_for.h>`, `<oneapi/tbb/task_group.h>`. Semantics:
> the callable runs **eventually** on some worker; ordering across tasks is not
> guaranteed unless synchronized.

For unstructured fork-join and explicit task creation, see Part 2 (`task_group`,
continuations). For the deque mechanics that make tasks cheap to move between
workers, continue to Part 0.3.

---

## Summary

- A **task** is a lightweight function object; a **worker** is a long-lived OS
  thread (~one per core) that executes tasks from its deque.
- Tasks spawn in **~tens of ns**; threads in **~µs** — never map iterations 1:1
  to threads.
- The runtime keeps concurrency ≈ **hardware_concurrency()**, so nested TBB
  parallelism **composes** without oversubscription.
- **Blocking** inside a task removes a worker from the pool and kills scaling;
  keep bodies compute-bound.
- Use algorithms and `task_group` to express work; let the scheduler (Part 0.3)
  decide mapping to workers.

Next: [0.3 — The work-stealing scheduler](03-work-stealing-scheduler.md)
