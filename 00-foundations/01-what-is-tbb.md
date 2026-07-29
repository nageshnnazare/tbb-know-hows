# 0.1 — What Is TBB? The Big Picture

Intel oneAPI Threading Building Blocks (**oneTBB**) is, mechanically, **a
task scheduler with a library of parallel patterns and containers layered on
top of it**:

```
   you write:   "these iterations are independent"
   TBB does:    split into tasks → steal across cores → balance the load
```

You never spawn a thread, never pin work to a core, never write a load-balancing
loop. You express *what* can run in parallel; the runtime decides *when* and *on
which thread*. Everything else in this guide — `parallel_for`, `concurrent_hash_map`,
the Flow Graph — is machinery built on that one idea.

---

## 0.1.1 The layer cake

![The oneTBB layer cake: algorithms/containers/flow-graph over the task scheduler over threads](figures/tbb-stack.svg)

TBB is not one thing; it is a stack, and each layer is useful on its own:

```
   ┌───────────────────────────────────────────────────────────────┐
   │ Parallel algorithms · Flow Graph · Concurrent containers      │  ← what you call
   ├───────────────────────────────────────────────────────────────┤
   │ Generic parallel patterns (parallel_for / reduce / pipeline)  │
   ├───────────────────────────────────────────────────────────────┤
   │ TASK SCHEDULER — work-stealing, arenas, tasks                 │  ← the engine
   ├───────────────────────────────────────────────────────────────┤
   │ Scalable memory allocator (tbbmalloc)                         │
   ├───────────────────────────────────────────────────────────────┤
   │ OS threads · hardware cores                                   │
   └───────────────────────────────────────────────────────────────┘
```

The layer that makes TBB *TBB* is the **task scheduler** (Part 0.3). Almost
everything above it is a convenience that decomposes your problem into tasks and
hands them to that scheduler.

> **The key idea ▸** TBB is *composable*. Because every algorithm ultimately
> becomes tasks on one shared scheduler, you can nest a `parallel_for` inside
> another `parallel_for`, call a TBB-parallel library from your own
> `parallel_reduce`, and **not** oversubscribe the machine. The scheduler still
> only runs ~one worker per core. This is the single biggest advantage over
> hand-rolled `std::thread` code, where nesting explodes into too many threads.

---

## 0.1.2 Why not just use threads?

You *can* parallelize with `std::thread` directly. The problem is that raw
threads push three hard problems onto you:

```
   1. HOW MANY?   spawn too few → idle cores; too many → context-switch churn.
   2. WHO DOES WHAT?  static split → stragglers stall everyone (load imbalance).
   3. COMPOSITION?   a parallel function calling another → thread explosion.
```

TBB answers all three automatically:

| Problem            | Raw `std::thread`                | TBB                                   |
|--------------------|----------------------------------|---------------------------------------|
| Thread count       | you pick (and usually get it wrong) | ~one worker per core, set once     |
| Load balancing     | manual, static                   | automatic, dynamic (work-stealing)    |
| Nested parallelism | oversubscribes                   | composes on one scheduler             |
| Work-unit cost     | thread create ≈ microseconds     | task spawn ≈ tens of nanoseconds      |

Part 0.2 makes the tasks-vs-threads distinction concrete.

---

## 0.1.3 The three things you actually tune

TBB does the scheduling, but you own three decisions that decide whether it
scales:

```
   ┌──────────────┬──────────────────────────────────────────────────────────┐
   │ Decomposition│ Is the work really independent? (or a reduction / graph) │
   │ Grain size   │ How small a chunk is worth a task? (Part 0.4)            │
   │ Data layout  │ Are threads fighting over cache lines? (false sharing,   │
   │              │ Part 5.2) or a global allocator lock? (Part 5.1)         │
   └──────────────┴──────────────────────────────────────────────────────────┘
```

Get these wrong and you will see the hallmark disappointment of naive
parallelism: **more cores, no faster** — or even slower. This guide keeps
returning to these three.

---

## 0.1.4 A first, complete example

The canonical "hello, parallelism": scale every element of an array. No threads,
no locks.

```cpp
// build: g++ -std=c++17 -O2 hello_tbb.cpp -ltbb -o hello_tbb
#include <oneapi/tbb/parallel_for.h>
#include <oneapi/tbb/blocked_range.h>
#include <vector>
#include <cstdio>

int main() {
    std::vector<double> a(10'000'000, 1.0);

    // Describe WHAT is parallel: the index range [0, a.size()).
    tbb::parallel_for(
        tbb::blocked_range<size_t>(0, a.size()),
        [&](const tbb::blocked_range<size_t>& r) {
            // The runtime hands each task a SUBRANGE r; iterate it serially.
            for (size_t i = r.begin(); i != r.end(); ++i)
                a[i] = a[i] * 2.0 + 1.0;
        });

    std::printf("a[0]=%g a[last]=%g\n", a.front(), a.back());
    return 0;
}
```

What happened underneath:

```
   blocked_range[0, 10M)
        └─ split ─┬─ [0, 5M) ─┬─ [0, 2.5M) ── task → worker 0
                  │           └─ [2.5M, 5M) ─ task → worker 1
                  └─ [5M, 10M) ─ ... splitting continues until grainsize
```

The range recursively splits into subranges; each becomes a **task**; the
**work-stealing scheduler** spreads those tasks across a worker per core and
rebalances if one finishes early. You wrote a serial `for` over a subrange and
got a parallel program.

> **Pitfall ▸** Notice the body loops over `r` with a plain `for`. A common
> beginner error is to ignore `r` and re-loop over the whole array, or to write
> to `a[i]` from a lambda that captures the wrong thing. The body must process
> **only its subrange** and touch only independent data (see Part 1.1).

---

## 0.1.5 When TBB is (and isn't) the right tool

```
   ✓ CPU-bound work with lots of independent or reducible computation
   ✓ Irregular / recursive parallelism where static splitting fails
   ✓ Nested or library parallelism that must compose without oversubscription
   ✓ Data structures shared across threads (concurrent containers)

   ✗ Pure I/O-bound waiting — you want async I/O, not CPU workers (blocking a
     TBB worker starves the pool; see Part 2.2 for how to isolate it)
   ✗ GPU offload — that's SYCL/oneAPI DPC++, a different tool
   ✗ Trivially small workloads — the parallel overhead dwarfs the work
```

TBB and the C++17/20 parallel STL (`std::execution::par`) overlap; on many
platforms the parallel STL is *implemented on top of TBB*. TBB gives you finer
control (arenas, partitioners, containers, Flow Graph) than the STL exposes.

---

## Summary

- oneTBB is a **task scheduler** plus a library of parallel algorithms,
  concurrent containers, and a Flow Graph built on it.
- You express *what* is parallel; the runtime decides *when/where* by splitting
  work into **tasks** and load-balancing them with **work-stealing**.
- It is **composable**: nested parallelism runs on one shared scheduler instead
  of oversubscribing the machine — the key win over raw threads.
- The three things *you* tune are **decomposition**, **grain size**, and **data
  layout**; getting them wrong is why naive parallel code fails to scale.
- Reach for TBB for CPU-bound, independent/reducible, or irregular work — not
  for I/O waiting or GPU offload.

Next: [0.2 — Tasks vs threads](02-tasks-vs-threads.md)
