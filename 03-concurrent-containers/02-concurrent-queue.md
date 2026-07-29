# 3.2 — concurrent_queue

`concurrent_queue<T>` is an **unbounded, FIFO, multi-producer multi-consumer
(MPMC)** queue. Producers call `push`; consumers call `try_pop` (non-blocking)
or, in the bounded variant (Part 3.5), blocking `pop`. It is the standard TBB
building block for **producer–consumer** pipelines, work handoff between
`parallel_for` tasks, and background logging — without wrapping
`std::queue` in a mutex.

Mechanically it is a **lock-free (or mostly lock-free) linked structure** with
per-operation linearizability: each `push`/`pop` appears to happen at a single
instant in the global order.

---

## 3.2.1 The MPMC pattern

![concurrent_queue with multiple producers and consumers](figures/concurrent-queue.svg)

```
   producer 0 ──push──┐
   producer 1 ──push──┼──▶  concurrent_queue  ──try_pop──▶ consumer A
   producer 2 ──push──┘         (FIFO)           try_pop──▶ consumer B
```

Unlike a mutex + `std::queue`, producers and consumers do not serialize on
every operation. Under low contention, throughput scales; under heavy
contention, cache-line bouncing on queue head/tail still limits you (see
[Part 5.2](../05-memory/02-cache-alignment-false-sharing.md)).

> **The API ▸**
> ```cpp
> #include <oneapi/tbb/concurrent_queue.h>
>
> void push(const T& value);       // enqueue; unbounded — never blocks for space
> void push(T&& value);
> bool try_pop(T& destination);    // dequeue if available; false if empty
> bool empty() const;              // approximate — see pitfalls below
> void clear();                    // drain — not safe vs concurrent push/pop
> ```
> There is **no** `front()`, **no** `back()`, and **no** blocking `pop()` on
> the unbounded queue. Design around **`try_pop`**.

---

## 3.2.2 try_pop: the only safe consumer API

> **The API ▸**
> ```cpp
> bool try_pop(T& destination);
> ```
> If an element is available, moves/copies it into `destination` and returns
> `true`. If the queue is empty, returns `false` **without blocking** and
> leaves `destination` unchanged.

The consumer loop pattern:

```cpp
T item;
while (queue.try_pop(item))
    process(item);
```

For **blocking** wait when empty, either spin with backoff, use a
`condition_variable` alongside the queue, or switch to
`concurrent_bounded_queue` (Part 3.5) whose `pop()` blocks.

> **Pitfall ▸** Never write logic like:
> ```cpp
> if (!queue.empty()) {           // ✗ race: empty() can flip before pop
>     T x = queue.front();        // ✗ no such API — and would be racy anyway
>     queue.try_pop(x);
> }
> ```
> **`empty()` is not a synchronization point.** Between `empty()` and
> `try_pop`, another thread may pop the last element or push a new one. The
> only correct non-blocking dequeue is **`try_pop` in a loop**.

---

## 3.2.3 unsafe_size and unsafe_begin

> **The API ▸**
> ```cpp
> size_t unsafe_size() const;   // approximate count; not for control flow
> iterator unsafe_begin();        // snapshot iterator; racy under concurrency
> iterator unsafe_end();
> ```
> **`unsafe_*` means exactly that.** `unsafe_size()` can be wrong before you
> read it; `unsafe_begin()`/`unsafe_end()` do not lock the queue — concurrent
> `push`/`try_pop` can invalidate the walk. Use only for **best-effort
> diagnostics** or when you've quiesced all producers and consumers.

Do not implement "wait until size >= N" with `unsafe_size`. Do not use
`size()`-based back-pressure on the unbounded queue — it has no capacity limit
and the size is unreliable for synchronization.

---

## 3.2.4 Example: producer–consumer with parallel_for

```cpp
// g++ -std=c++17 -O2 concurrent_queue_demo.cpp -ltbb
#include <oneapi/tbb/concurrent_queue.h>
#include <oneapi/tbb/parallel_for.h>
#include <oneapi/tbb/blocked_range.h>
#include <cstdio>

struct Job { int id; double payload; };

int main() {
    tbb::concurrent_queue<Job> queue;

    // Producers: parallel_for pushes jobs.
    tbb::parallel_for(
        tbb::blocked_range<int>(0, 4),
        [&](const tbb::blocked_range<int>& r) {
            for (int p = r.begin(); p != r.end(); ++p) {
                for (int i = 0; i < 25'000; ++i)
                    queue.push({p * 100'000 + i, i * 0.1});
            }
        });

    // Consumer: single thread drains with try_pop (extend to thread pool as needed).
    long long sum = 0;
    Job job;
    while (queue.try_pop(job))
        sum += job.id;

    std::printf("drained sum=%lld\n", sum);
    return 0;
}
```

Multiple consumer threads are fine — each `try_pop` consumes at most one
element; two threads never get the same item. Ordering is **FIFO globally**
for a single consumer; with multiple consumers, each item still comes out
once, but which consumer gets the next item is non-deterministic.

> **Under the hood ▸** The implementation uses a linked list of nodes allocated
> from the scalable allocator ([Part 5.1](../05-memory/01-scalable-allocator.md)).
> High push/pop rates allocate many small nodes — tune batching (push fewer,
> larger messages) if allocator traffic shows up in profiles.

**Trade-offs ▸**
- ✓ Simple MPMC FIFO; no capacity limit; `try_pop` avoids deadlock on empty.
- ✗ Unbounded → memory grows if producers outrun consumers; no peek; no
  reliable size; `clear()` vs concurrent access is unsafe.

**Tuning ▸** Batch work items into structs to reduce push/pop frequency. If
consumers idle while producers flood memory, use
[concurrent_bounded_queue](05-bounded-and-priority-queues.md) for back-pressure.

For keyed lookup instead of FIFO, see [concurrent_hash_map](03-concurrent-hash-map.md).

---

## Summary

- `concurrent_queue` is an **unbounded MPMC FIFO** — use `push` and
  **`try_pop`**, not `front`/`back` (they don't exist).
- **`empty()` and `unsafe_size()` are racy** — never use them to gate a pop;
  loop on `try_pop` instead.
- **`unsafe_begin`/`unsafe_end`** are for quiesced or diagnostic walks only.
- Classic **producer–consumer** pattern: parallel producers `push`, one or
  more consumers `try_pop` (or use bounded queue for blocking/back-pressure).

Next: [3.3 — concurrent_hash_map](03-concurrent-hash-map.md)
