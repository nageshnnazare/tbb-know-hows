# oneTBB API Cheat Sheet

Dense, scannable reference for the modern **oneTBB** API (`oneapi::tbb`, aliased as
`tbb::`). See [README](../README.md) for the full learning path.

---

## Namespace note

| Style | Usage |
|-------|-------|
| Modern (this guide) | `#include <oneapi/tbb/parallel_for.h>` → `tbb::parallel_for` |
| Legacy | `#include <tbb/parallel_for.h>` — still works on many installs via compatibility headers |
| Actual namespace | `oneapi::tbb`; many builds provide `namespace tbb = oneapi::tbb;` |

All examples assume C++17, `-ltbb`, and the `tbb::` prefix.

---

## Parallel algorithms

| Name | Header | Purpose | Part |
|------|--------|---------|------|
| `parallel_for` | `<oneapi/tbb/parallel_for.h>` | Parallel loop over a `blocked_range` or random-access range | [1.1](../01-parallel-algorithms/01-parallel-for.md) |
| `parallel_reduce` | `<oneapi/tbb/parallel_reduce.h>` | Map-reduce with local accumulate + join functor | [1.2](../01-parallel-algorithms/02-parallel-reduce.md) |
| `parallel_deterministic_reduce` | `<oneapi/tbb/parallel_reduce.h>` | Bit-identical reduction order (slower) | [7.3](../07-advanced-performance/03-deterministic-reduce.md) |
| `parallel_scan` | `<oneapi/tbb/parallel_scan.h>` | Prefix scan (inclusive/exclusive) in parallel | [1.3](../01-parallel-algorithms/03-parallel-scan.md) |
| `parallel_sort` | `<oneapi/tbb/parallel_sort.h>` | Parallel quicksort on random-access iterators | [1.4](../01-parallel-algorithms/04-parallel-sort.md) |
| `parallel_invoke` | `<oneapi/tbb/parallel_invoke.h>` | Run 2–10 callables concurrently, join all | [1.5](../01-parallel-algorithms/05-parallel-invoke.md) |
| `parallel_pipeline` | `<oneapi/tbb/parallel_pipeline.h>` | Linear token pipeline with serial/parallel filters | [1.6](../01-parallel-algorithms/06-parallel-pipeline.md) |
| `blocked_range<T>` | `<oneapi/tbb/blocked_range.h>` | Half-open iteration subrange with optional grainsize | [0.4](../00-foundations/04-ranges-and-grain-size.md) |
| `blocked_range2d/3d` | `<oneapi/tbb/blocked_range2d.h>` etc. | Multi-dimensional blocked ranges | [0.4](../00-foundations/04-ranges-and-grain-size.md) |
| `auto_partitioner` | `<oneapi/tbb/partitioner.h>` | Adaptive range splitting (default) | [0.5](../00-foundations/05-partitioners.md) |
| `simple_partitioner` | `<oneapi/tbb/partitioner.h>` | Split while range > grainsize | [0.5](../00-foundations/05-partitioners.md) |
| `static_partitioner` | `<oneapi/tbb/partitioner.h>` | Pre-split into ~P chunks | [0.5](../00-foundations/05-partitioners.md) |
| `affinity_partitioner` | `<oneapi/tbb/partitioner.h>` | Sticky subrange → worker affinity | [0.5](../00-foundations/05-partitioners.md) |
| `filter` / `filter_mode` | `<oneapi/tbb/parallel_pipeline.h>` | Pipeline stage body + serial/parallel mode | [1.6](../01-parallel-algorithms/06-parallel-pipeline.md) |

---

## Task programming

| Name | Header | Purpose | Part |
|------|--------|---------|------|
| `task_group` | `<oneapi/tbb/task_group.h>` | Unstructured fork-join; `run` / `wait` | [2.1](../02-task-programming/01-task-group.md) |
| `task_group_context` | `<oneapi/tbb/task_group.h>` | Cancellation/isolation context for task groups | [2.3](../02-task-programming/03-task-scheduler-and-control.md) |
| `task_arena` | `<oneapi/tbb/task_arena.h>` | Sub-pool with constrained parallelism | [2.2](../02-task-programming/02-task-arena.md) |
| `this_task_arena` | `<oneapi/tbb/task_arena.h>` | Current arena queries and `execute` | [2.2](../02-task-programming/02-task-arena.md) |
| `global_control` | `<oneapi/tbb/global_control.h>` | Process-wide caps (threads, stack size) | [7.1](../07-advanced-performance/01-global-control.md) |
| `task_scheduler_observer` | `<oneapi/tbb/task_scheduler_observer.h>` | Hooks on worker entry/exit | [7.2](../07-advanced-performance/02-task-scheduler-observer.md) |

---

## Concurrent containers

| Name | Header | Purpose | Part |
|------|--------|---------|------|
| `concurrent_vector` | `<oneapi/tbb/concurrent_vector.h>` | Growable array; stable iterators on `push_back` | [3.1](../03-concurrent-containers/01-concurrent-vector.md) |
| `concurrent_queue` | `<oneapi/tbb/concurrent_queue.h>` | Unbounded MPMC queue | [3.2](../03-concurrent-containers/02-concurrent-queue.md) |
| `concurrent_bounded_queue` | `<oneapi/tbb/concurrent_queue.h>` | Bounded MPMC queue; blocks or fails on full | [3.5](../03-concurrent-containers/05-bounded-and-priority-queues.md) |
| `concurrent_priority_queue` | `<oneapi/tbb/concurrent_priority_queue.h>` | Thread-safe priority queue | [3.5](../03-concurrent-containers/05-bounded-and-priority-queues.md) |
| `concurrent_hash_map` | `<oneapi/tbb/concurrent_hash_map.h>` | Sharded hash map with accessor locking | [3.3](../03-concurrent-containers/03-concurrent-hash-map.md) |
| `concurrent_unordered_map` | `<oneapi/tbb/concurrent_unordered_map.h>` | Concurrent unordered map (iterator invalidation rules differ) | [3.4](../03-concurrent-containers/04-concurrent-unordered-map.md) |

---

## Synchronization

| Name | Header | Purpose | Part |
|------|--------|---------|------|
| `mutex` | `<oneapi/tbb/mutex.h>` | Standard TBB mutex with `scoped_lock` | [4.1](../04-synchronization/01-mutexes.md) |
| `recursive_mutex` | `<oneapi/tbb/recursive_mutex.h>` | Reentrant mutex | [4.1](../04-synchronization/01-mutexes.md) |
| `spin_mutex` | `<oneapi/tbb/spin_mutex.h>` | Spin-wait lock for very short critical sections | [4.2](../04-synchronization/02-spin-and-queuing-mutex.md) |
| `queuing_mutex` | `<oneapi/tbb/queuing_mutex.h>` | FIFO fair mutex; avoids starvation | [4.2](../04-synchronization/02-spin-and-queuing-mutex.md) |
| `reader_writer_lock` | `<oneapi/tbb/reader_writer_lock.h>` | Shared (read) / exclusive (write) lock | [4.3](../04-synchronization/03-reader-writer-locks.md) |
| `atomic` | `<oneapi/tbb/atomic.h>` | Atomic types and fetch operations (prefer `std::atomic` in new code where equivalent) | [4.4](../04-synchronization/04-atomics.md) |

---

## Memory & thread-local

| Name | Header | Purpose | Part |
|------|--------|---------|------|
| `scalable_allocator` | `<oneapi/tbb/scalable_allocator.h>` | STL allocator backed by tbbmalloc per-thread pools | [5.1](../05-memory/01-scalable-allocator.md) |
| `tbb_allocator` | `<oneapi/tbb/tbb_allocator.h>` | General TBB allocator (scalable when tbbmalloc linked) | [5.1](../05-memory/01-scalable-allocator.md) |
| `cache_aligned_allocator` | `<oneapi/tbb/cache_aligned_allocator.h>` | Aligns allocations to cache-line boundaries | [5.2](../05-memory/02-cache-alignment-false-sharing.md) |
| `enumerable_thread_specific` | `<oneapi/tbb/enumerable_thread_specific.h>` | Per-thread copy of `T`; enumerable + combinable | [5.3](../05-memory/03-thread-local-storage.md) |
| `combinable` | `<oneapi/tbb/combinable.h>` | Lazy thread-local foldable accumulator | [5.3](../05-memory/03-thread-local-storage.md) |
| tbbmalloc_proxy | link `-ltbbmalloc_proxy` | Interpose malloc/free/new/delete globally | [5.1](../05-memory/01-scalable-allocator.md) |

---

## Flow Graph (`tbb::flow`)

| Name | Header | Purpose | Part |
|------|--------|---------|------|
| `graph` | `<oneapi/tbb/flow_graph.h>` | Owns nodes; `wait_for_all()` | [6.1](../06-flow-graph/01-flow-graph-intro.md) |
| `make_edge` | `<oneapi/tbb/flow_graph.h>` | Connect sender → receiver | [6.1](../06-flow-graph/01-flow-graph-intro.md) |
| `input_node` | `<oneapi/tbb/flow_graph.h>` | Message source; `activate()` / `flow_control` | [6.1](../06-flow-graph/01-flow-graph-intro.md) |
| `function_node` | `<oneapi/tbb/flow_graph.h>` | Transform message; concurrency + optional priority | [6.2](../06-flow-graph/02-function-and-buffer-nodes.md) |
| `continue_node` | `<oneapi/tbb/flow_graph.h>` | Dependency trigger via `continue_msg` | [6.2](../06-flow-graph/02-function-and-buffer-nodes.md) |
| `multifunction_node` | `<oneapi/tbb/flow_graph.h>` | Route input to multiple output ports | [6.2](../06-flow-graph/02-function-and-buffer-nodes.md) |
| `buffer_node` | `<oneapi/tbb/flow_graph.h>` | Unordered buffer | [6.2](../06-flow-graph/02-function-and-buffer-nodes.md) |
| `queue_node` | `<oneapi/tbb/flow_graph.h>` | FIFO buffer | [6.2](../06-flow-graph/02-function-and-buffer-nodes.md) |
| `priority_queue_node` | `<oneapi/tbb/flow_graph.h>` | Priority-ordered buffer | [6.2](../06-flow-graph/02-function-and-buffer-nodes.md) |
| `sequencer_node` | `<oneapi/tbb/flow_graph.h>` | Reorder stream by key | [6.2](../06-flow-graph/02-function-and-buffer-nodes.md) |
| `overwrite_node` | `<oneapi/tbb/flow_graph.h>` | Keep latest message only | [6.2](../06-flow-graph/02-function-and-buffer-nodes.md) |
| `join_node` | `<oneapi/tbb/flow_graph.h>` | Fan-in to `tuple`; queueing/reserving/key_matching | [6.3](../06-flow-graph/03-join-and-split-nodes.md) |
| `split_node` | `<oneapi/tbb/flow_graph.h>` | Fan-out tuple to ports | [6.3](../06-flow-graph/03-join-and-split-nodes.md) |
| `broadcast_node` | `<oneapi/tbb/flow_graph.h>` | Send each message to all successors | [6.3](../06-flow-graph/03-join-and-split-nodes.md) |
| `limiter_node` | `<oneapi/tbb/flow_graph.h>` | Cap in-flight messages; `decrementer()` | [6.4](../06-flow-graph/04-priorities-and-limiters.md) |
| `node_priority_t` | `<oneapi/tbb/flow_graph.h>` | Scheduling bias for functional nodes | [6.4](../06-flow-graph/04-priorities-and-limiters.md) |
| `input_port<N>` / `output_port<N>` | `<oneapi/tbb/flow_graph.h>` | Port accessors for multi-port nodes | [6.3](../06-flow-graph/03-join-and-split-nodes.md) |

---

## Compile & link

```bash
# Minimal
g++ -std=c++17 -O2 app.cpp -ltbb -o app

# With scalable allocator (explicit scalable_allocator / tbb_allocator)
g++ -std=c++17 -O2 app.cpp -ltbb -ltbbmalloc -o app

# Transparent malloc replacement
g++ -std=c++17 -O2 app.cpp -ltbbmalloc_proxy -ltbbmalloc -ldl -o app

# Debug build
g++ -std=c++17 -g -DTBB_USE_DEBUG app.cpp -ltbb_debug -o app
```

| Platform | Install |
|----------|---------|
| Ubuntu/Debian | `sudo apt install libtbb-dev libtbbmalloc2` |
| Fedora | `sudo dnf install tbb tbb-devel` |
| macOS | `brew install tbb` |
| vcpkg | `vcpkg install tbb` |
| Conan | `requires("tbb/2022.x")` |

| Library | When to link |
|---------|--------------|
| `-ltbb` | Always — scheduler, algorithms, containers, flow graph |
| `-ltbbmalloc` | Explicit `scalable_allocator` / `tbb_allocator` |
| `-ltbbmalloc_proxy` | Replace global malloc/new without source changes |
| `-ldl` | Often required with tbbmalloc_proxy on Linux |

---

Back to [README](../README.md)
