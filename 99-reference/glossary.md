# Glossary

Alphabetized terms for the [Intel oneTBB Mastery Guide](../README.md). Each entry
links to the Part where the concept is developed.

---

**affinity_partitioner** — A [partitioner](../00-foundations/05-partitioners.md) that
records which worker executed each subrange and nudges subsequent splits toward the
same mapping. Improves cache reuse when loops revisit the same data across passes;
less adaptive than `auto_partitioner` under load imbalance.

**arena** — An isolation domain for the task scheduler: a bounded set of workers
and a task pool. See [task_arena](../02-task-programming/02-task-arena.md). Nested
arenas let you cap parallelism for a subsystem without changing the global pool.

**associativity** — The property `(a ⊕ b) ⊕ c = a ⊕ (b ⊕ c)` for a reduction
operator. Floating-point addition is **not** associative; [parallel_reduce](../01-parallel-algorithms/02-parallel-reduce.md)
may produce slightly different bits across runs. Use
[deterministic reduce](../07-advanced-performance/03-deterministic-reduce.md) when
reproducibility matters.

**atomic** — A type whose read-modify-write operations are indivisible with respect
to other threads. TBB provides [atomics](../04-synchronization/04-atomics.md); modern
code often uses `std::atomic`. Atomics fix correctness, not false sharing.

**auto_partitioner** — Default [partitioner](../00-foundations/05-partitioners.md):
recursively splits ranges while work-stealing indicates imbalance, stopping near
the grainsize. Best general choice when grain size is unknown.

**blocked_range** — A half-open index subrange `[begin, end)` with optional
[grainsize](../00-foundations/04-ranges-and-grain-size.md). The unit `parallel_for`
and friends split recursively into tasks.

**cache line** — The smallest unit of cache coherency, typically 64 bytes. Writes
invalidate the entire line. See [cache alignment & false sharing](../05-memory/02-cache-alignment-false-sharing.md).

**combinable** — A lightweight thread-local foldable accumulator, similar to
[enumerable_thread_specific](../05-memory/03-thread-local-storage.md) but with lazy
`local()` and no default-construct requirement on first touch. Header:
`<oneapi/tbb/combinable.h>`.

**composability** — The property that nested TBB parallelism shares one scheduler
without oversubscribing threads. Described in [What is TBB?](../00-foundations/01-what-is-tbb.md).
Raw `std::thread` nesting typically lacks this.

**concurrent container** — A container designed for concurrent access without an
external lock on every operation, e.g.
[concurrent_vector](../03-concurrent-containers/01-concurrent-vector.md),
[concurrent_queue](../03-concurrent-containers/02-concurrent-queue.md),
[concurrent_hash_map](../03-concurrent-containers/03-concurrent-hash-map.md).

**continuation** — A task scheduled to run after predecessor tasks complete; the
dependency model in [task_group and continuations](../02-task-programming/04-continuations-and-dependencies.md)
and Flow Graph edges.

**CAS** — Compare-and-swap: atomically update a word if it still holds an expected
value. Foundation of lock-free algorithms; covered under [atomics](../04-synchronization/04-atomics.md).

**deque** — Double-ended queue. Each TBB worker owns a task deque for LIFO local
pop and FIFO steal from other workers. See [work-stealing scheduler](../00-foundations/03-work-stealing-scheduler.md).

**deterministic reduce** — `parallel_deterministic_reduce`: fixed reduction tree
for reproducible floating-point results at a performance cost. See
[Part 7.3](../07-advanced-performance/03-deterministic-reduce.md).

**enumerable_thread_specific** — Per-thread copy of `T` with `local()`,
iteration, and `combine()`. The standard pattern for lock-free accumulation. See
[Part 5.3](../05-memory/03-thread-local-storage.md).

**false sharing** — Independent variables on the same cache line cause coherence
traffic when different cores write them. Can make parallel code slower than serial.
See [Part 5.2](../05-memory/02-cache-alignment-false-sharing.md).

**filter** — A stage body in [parallel_pipeline](../01-parallel-algorithms/06-parallel-pipeline.md)
with a mode: parallel, serial_in_order, serial_out_of_order.

**Flow Graph** — Dataflow framework: nodes (actors), edges (channels), reactive
execution when messages arrive. See [Part 6](../06-flow-graph/01-flow-graph-intro.md).

**fork-join** — Split work into subtasks, run concurrently, merge results. Model
for `parallel_reduce`, `task_group`, and join nodes.

**grain size** — Minimum subrange size the scheduler will not split further.
Dominates task overhead vs load balance. See [ranges and grain size](../00-foundations/04-ranges-and-grain-size.md).

**hardware_concurrency** — `std::thread::hardware_concurrency()`: OS-reported
logical processor count; default TBB worker target unless overridden by
[global_control](../07-advanced-performance/01-global-control.md).

**join_node** — Flow Graph fan-in node forming a `tuple` when inputs arrive.
Policies: queueing, reserving, key_matching. See
[Part 6.3](../06-flow-graph/03-join-and-split-nodes.md).

**lock-free** — Progress guaranteed system-wide even if some threads stall; typically
implemented with CAS loops. Concurrent containers and tbbmalloc fast paths use
lock-free techniques; not the same as wait-free.

**oversubscription** — More runnable threads than cores, causing context-switch
overhead. TBB avoids this via a fixed worker pool; see [tasks vs threads](../00-foundations/02-tasks-vs-threads.md).

**parallel_for** — Parallel loop over a range; the canonical TBB algorithm. See
[Part 1.1](../01-parallel-algorithms/01-parallel-for.md).

**parallel_pipeline** — Linear pipeline of filters processing a stream of tokens.
See [Part 1.6](../01-parallel-algorithms/06-parallel-pipeline.md).

**parallel_reduce** — Parallel map-reduce with body-local accumulate and join
functor. See [Part 1.2](../01-parallel-algorithms/02-parallel-reduce.md).

**partitioner** — Controls how ranges split into tasks: auto, simple, static,
affinity. See [Part 0.5](../00-foundations/05-partitioners.md).

**proxy (tbbmalloc_proxy)** — Shared library interposing malloc/free/new/delete
to route through tbbmalloc. See [scalable allocator](../05-memory/01-scalable-allocator.md).

**range** — A splittable description of work, usually `blocked_range<T>`. Must
support a `split` constructor and empty test. See [Part 0.4](../00-foundations/04-ranges-and-grain-size.md).

**reduction** — Combining partial results with an associative operator (sum, min,
union). Implemented by `parallel_reduce`, ETS/combinable merge, or Flow Graph joins.

**scalable allocator** — STL allocator using tbbmalloc per-thread pools to avoid
global malloc contention. See [Part 5.1](../05-memory/01-scalable-allocator.md).

**scoped_lock** — RAII guard on a TBB mutex: acquire in constructor, release in
destructor. See [mutexes](../04-synchronization/01-mutexes.md).

**spin_mutex** — Mutex that busy-spins instead of sleeping; for very short critical
sections only. See [Part 4.2](../04-synchronization/02-spin-and-queuing-mutex.md).

**split (constructor)** — Range method invoked by the scheduler to create two
subranges; defines the decomposition tree. See [Part 0.4](../00-foundations/04-ranges-and-grain-size.md).

**task** — Lightweight unit of work scheduled by TBB, not an OS thread. See
[tasks vs threads](../00-foundations/02-tasks-vs-threads.md).

**task_arena** — Constrained worker sub-pool for isolating or limiting parallelism.
See [Part 2.2](../02-task-programming/02-task-arena.md).

**task_group** — Unstructured fork-join: `run()` tasks, `wait()` for completion.
See [Part 2.1](../02-task-programming/01-task-group.md).

**token** — In `parallel_pipeline`, one item flowing through all filters; also the
count returned via `limiter_node::decrementer()`. See [Part 6.4](../06-flow-graph/04-priorities-and-limiters.md).

**work-stealing** — Idle workers take the oldest task from another worker's deque.
Core TBB scheduling mechanism. See [Part 0.3](../00-foundations/03-work-stealing-scheduler.md).

**worker** — A long-lived OS thread in the TBB pool (~one per core) that executes
tasks from its deque. Distinct from a **task**.

---

Back to [README](../README.md)
