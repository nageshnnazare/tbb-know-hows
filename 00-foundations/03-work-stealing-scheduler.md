# 0.3 — The Work-Stealing Scheduler

Part 0.2 established that **tasks** are cheap and **workers** are scarce. This
chapter dissects the algorithm that connects them: the **work-stealing scheduler**
that keeps every core busy with minimal locking and almost no global contention.

---

## 0.3.1 One deque per worker

Each worker thread owns a **double-ended queue (deque)** of ready-to-run task
pointers. Producers and consumers are usually the **same** worker (the one
executing a parent task that spawns children), so the hot path is **local**:

```
   worker 2 deque                         worker 5 deque
   ┌───┬───┬───┬───┐                       ┌───┬───┐
   │ T │ T │ T │ T │  ← "top" (young end)  │ T │ T │
   └───┴───┴───┴───┘                       └───┴───┘
     ↑                                       ↑
   bottom (old end)                        bottom (old end)
   owner pushes/pops here                  idle worker steals HERE
```

![Each worker owns a deque; owners push/pop the young end, thieves steal from the old end](figures/work-stealing.svg)

> **Under the hood ▸** The deque is the Chase-Lev / Arora-Blumofe-Plaxton style
> structure: the owner operates on one end with minimal synchronization; a thief
> accessing the opposite end races only occasionally with the owner. There is **no
> single global task queue** — that would be a lock or atomic storm at scale.

When you call `parallel_for`, the calling thread participates as a worker: it
pushes split tasks onto its deque and executes them, same as any other worker.

---

## 0.3.2 Local work: LIFO (young end)

The **owner** worker pushes newly spawned tasks and pops work for itself from the
**same end** of the deque — traditionally the **top**, the **youngest** tasks:

```
   spawn child tasks:  push →  [ T_new | T_a | T_b | T_c ]
   execute next:       pop  ←  same end (LIFO among local work)
```

**Why LIFO?**

1. **Cache locality** — the most recently spawned child likely shares fresh data
   still in L1/L2 with the parent that just created it.
2. **Depth-first execution** — finish subtrees before siblings, which tends to
   reduce live task memory and matches divide-and-conquer recursion.

This is the opposite of a fair FIFO queue — and that is intentional. Local LIFO
keeps each worker **hot** on its slice of the problem.

---

## 0.3.3 Stealing: FIFO from the victim's old end

When a worker's deque is **empty**, it becomes a **thief**. It picks another
worker (victim) and tries to **steal** from the **opposite** end — the **bottom**,
where the **oldest** tasks live:

```
   victim deque:   bottom → [ T_old BIG | ... | T_young ] ← top
                              ↑
   thief takes ONE task from here (the largest remaining chunk)
```

**Why steal the oldest (biggest) task?**

| Steal youngest | Steal oldest ✓ |
|----------------|----------------|
| tiny remainder slice | coarse chunk ≈ half the victim's remaining range |
| thief finishes fast, steals again → contention | thief stays busy longer |
| victim did all splitting for nothing | splits amortize on the busy worker |

Stealing the big end is the central insight of **Cilk-style** work stealing: the
worker that created many small tasks keeps the fine-grained pieces; the idle worker
grabs a coarse piece and only splits if it needs to.

> **Under the hood ▸** A steal is a **single** task pointer exchange at the
> victim's bottom. Most scheduling steps never touch another worker's deque.
> **Stealing is the exception, not the rule** — on uniform workloads most workers
> never steal after initial distribution.

---

## 0.3.4 Random victim selection

When worker *i* needs to steal, it does not scan all deques deterministically.
It probes **random** victims (pseudo-random seed per worker) until a steal
succeeds or it confirms no work exists:

```
   thief loop:
       victim = random_worker()
       if steal(bottom of victim.deque) succeeds → run stolen task
       else retry (with backoff / park if global idle)
```

Randomization spreads steal attempts across victims, avoiding **systematic
hot spots** where every idle worker hammers worker 0's deque.

**Trade-offs ▸** Random steal targets add negligible overhead when work is plentiful
and steal rarely happens. Under extreme imbalance (one straggler, everyone else
idle), multiple thieves may collide on the same victim — retries are cheap compared
to leaving cores idle.

---

## 0.3.5 The spawn tree and task DAG

Recursive splitting builds a **tree** of tasks — the **spawn tree**. Edges are
"parent spawned child"; the runtime schedules ready nodes, not necessarily in
tree order:

```
                    parallel_for root
                          │
              ┌───────────┴───────────┐
           [0, n/2)                 [n/2, n)
              │                         │
         ┌────┴────┐               ┌────┴────┐
      [0,n/4) [n/4,n/2)        ...       ...     ← leaves ≈ grain-sized chunks
         │       │
       execute execute   (worker pops LIFO locally)
```

More generally, **`task_group`** and Flow Graph (Part 6) produce a **DAG** —
join nodes wait on predecessors. The same deque + steal machinery applies: a task
becomes runnable when its dependencies complete and is then pushed on a deque.

The split tree from `blocked_range` (Part 0.4) is the most common spawn tree you
will see. **`auto_partitioner`** (Part 0.5) can defer splits until a thief needs
work, flattening the tree under load.

---

## 0.3.6 A steal in slow motion

Consider four workers, uniform `parallel_for`, grain size already reached on
worker 0:

```
   t=0   W0: [leaf leaf leaf leaf]   busy executing
         W1: [leaf leaf]             busy
         W2: []                        idle → steal
         W3: [leaf]                    busy

   t=1   W2 steals OLDEST leaf from W0's bottom
         W0: [leaf leaf leaf]         W2: [large half-range]  now both busy

   t=2   W1 finishes early
         W1: index → steal from W3 or W0
```

No central coordinator assigned "worker 2 handles indices 5000–9999." Assignment
emerged from **who split what** and **who stole what**. That is why TBB handles
**irregular** per-element cost: fast workers implicitly take more tasks via stealing.

> **Pitfall ▸** **`static_partitioner`** (Part 0.5) disables stealing by giving
> each worker a fixed slice. Uniform work → great cache behavior; **imbalanced**
> work → a idle core cannot help a straggler. Default **`auto_partitioner`** exists
> precisely so stealing can recover balance.

---

## 0.3.7 Observing the scheduler (conceptual)

You cannot see deques from application code, but the behavior has signatures in
profiles:

```
   ✓ good:  all cores busy until join; steal time ≪ compute time
   ✗ bad:   cores idle while one thread runs; high steal/sync in VTune
   ✗ bad:   one worker at 100% in kernel mutex — not stealing, blocking (Part 0.2)
```

`task_scheduler_observer` (Part 7.2) hooks worker entry/exit for custom
instrumentation. **`global_control`** (Part 7.1) caps workers when co-running with
other thread pools.

---

## 0.3.8 End-to-end: from parallel_for to steal

```cpp
// g++ -std=c++17 -O2 steal_demo.cpp -ltbb -o steal_demo
#include <oneapi/tbb/parallel_for.h>
#include <oneapi/tbb/blocked_range.h>
#include <vector>
#include <cmath>
#include <cstdio>

int main() {
    const size_t n = 50'000'000;
    std::vector<double> a(n, 1.0);

    // Variable per-index cost → imbalance → stealing likely on some runs
    tbb::parallel_for(
        tbb::blocked_range<size_t>(0, n, 10'000),  // grain: Part 0.4
        [&](const tbb::blocked_range<size_t>& r) {
            for (size_t i = r.begin(); i != r.end(); ++i) {
                double x = a[i];
                for (int k = 0; k < static_cast<int>(i & 7); ++k)
                    x = std::sqrt(x + 1.0);
                a[i] = x;
            }
        });  // default auto_partitioner → splits + steals as needed

    std::printf("done, a.back()=%g\n", a.back());
    return 0;
}
```

Mechanical sequence:

```
   1. Caller worker pushes root range task on its deque.
   2. Execute: split while is_divisible() && partitioner allows.
   3. Child tasks pushed locally (LIFO); parent may become idle or execute too.
   4. Fast workers drain deques; idle workers steal oldest tasks from victims.
   5. Leaf bodies run serially over subranges; join is implicit when deques empty.
```

Grain size sets the **smallest** leaf; the scheduler decides **how many** leaves
exist concurrently and **which worker** runs each via local pop + steal.

---

## 0.3.9 Relation to other layers

| Layer | Role |
|-------|------|
| **Range / grain** (0.4) | defines *what* to split and when to stop |
| **Partitioner** (0.5) | controls *how aggressively* to split vs steal |
| **Algorithms** (Part 1) | `parallel_for`, `parallel_reduce`, … build the spawn tree |
| **task_arena** (Part 2.2) | isolates deques/workers for subsets of threads |
| **Performance** (Part 7) | measure steal overhead, tune grain, avoid false sharing |

The scheduler is the constant. Everything above it is a different way to enqueue
work onto deques.

> **The API ▸** Scheduling is implicit — no public "steal" or "push task" in
> `parallel_for`. Explicit task injection:
>
> ```cpp
> #include <oneapi/tbb/task_group.h>
> tbb::task_group g;
> g.run([] { /* task body */ });
> g.wait();
> ```
>
> `run()` enqueues a task on the current arena's scheduler; execution follows the
> same deque/steal rules described here.

---

## Summary

- Each worker owns a **deque**; local **push/pop is LIFO** (young end) for cache-hot,
  depth-first execution.
- Idle workers **steal from the victim's old end (FIFO)**, taking the **largest**
  remaining task to minimize contention and balance load.
- **Random victim** selection avoids steal hot spots; stealing is **rare** on
  balanced workloads but essential for stragglers.
- Recursive splits form a **spawn tree** (or DAG for graphs); the scheduler never
  uses a global run queue.
- **`auto_partitioner`** + sensible **grain size** (Part 0.4) let stealing fix
  imbalance; **`static_partitioner`** turns it off.

Next: [0.4 — Ranges, splitting & grain size](04-ranges-and-grain-size.md)
