# 3.5 — Bounded & Priority Queues

Two specialised queues complete the TBB container toolbox:
**`concurrent_bounded_queue`** — FIFO with a **capacity ceiling** and
**blocking** push/pop for back-pressure — and **`concurrent_priority_queue`**
— a thread-safe **heap** where `try_pop` returns the **highest-priority**
element. Both live beside the unbounded [concurrent_queue](02-concurrent-queue.md)
from Part 3.2.

Pick bounded when producers must not outrun memory; pick priority when **order
by importance** matters more than strict arrival order.

---

## 3.5.1 concurrent_bounded_queue: capacity and blocking

```
   capacity = 4
   ┌───┬───┬───┬───┐
   │ 0 │ 1 │ 2 │ 3 │  FULL → push() blocks (back-pressure)
   └───┴───┴───┴───┘
        ▲           ▲
     pop()       try_push → false when full
   blocks if
   empty
```

> **The API ▸**
> ```cpp
> #include <oneapi/tbb/concurrent_queue.h>   // bounded_queue is here too
>
> void set_capacity(size_type cap);
> size_type capacity() const;
>
> void push(const T& value);              // blocks until space available
> void pop(T& destination);               // blocks until item available
> bool try_push(const T& value);          // false if full — non-blocking
> bool try_pop(T& destination);           // false if empty
> void abort_push(const T& value);        // push or discard if full (no block)
> ```
> Default capacity is **unlimited** until you call **`set_capacity`**. After
> that, **`push`** applies **back-pressure**: producers stall instead of
> allocating unbounded nodes.

> **Under the hood ▸** Shares implementation lineage with
> `concurrent_queue`, but adds a semaphore/counter for capacity. Blocking
> `push`/`pop` may park threads in the OS — **do not call blocking `pop` from
> a TBB worker** doing scheduler work unless you accept reduced parallelism
> ([Part 0.2 — Tasks vs threads](../00-foundations/02-tasks-vs-threads.md):
> blocking a worker starves the pool).

> **Pitfall ▸** `size()` on bounded queue is more usable than
> `unsafe_size` on the unbounded queue, but still avoid using **`size()` for
> control-flow synchronization** between threads — use `try_push`/`try_pop` or
> blocking operations.

---

## 3.5.2 concurrent_priority_queue: thread-safe heap

```
        push(5) push(1) push(10) push(3)
                    │
                    ▼
              max-heap (default)
                    │
        try_pop → 10, then 5, then 3, then 1
```

> **The API ▸**
> ```cpp
> #include <oneapi/tbb/concurrent_priority_queue.h>
>
> void push(const T& value);
> bool try_pop(T& destination);   // removes highest-priority per comparator
> bool empty() const;
> size_type size() const;
> ```
> Default comparator is **`std::less<T>`** → **max-heap** (largest `T` first).
> Use `concurrent_priority_queue<T, std::greater<T>>` for a min-heap.

> **Pitfall ▸** **Equal priorities have no defined order** among themselves —
> do not rely on FIFO fairness within the same priority level. Need strict
> FIFO at equal priority? Store `(priority, sequence)` tuples and compare
> sequence on tie-break.

There is **no blocking pop** — only `try_pop`. No iterators (like
`std::priority_queue`).

---

## 3.5.3 Example: bounded queue as a work buffer

A fixed-size buffer between fast producers and slower consumers — classic
**pipeline throttle**:

```cpp
// g++ -std=c++17 -O2 bounded_queue_demo.cpp -ltbb -pthread
#include <oneapi/tbb/concurrent_queue.h>
#include <thread>
#include <cstdio>

int main() {
    tbb::concurrent_bounded_queue<int> buffer;
    buffer.set_capacity(64);   // at most 64 outstanding items

    std::thread producer([&] {
        for (int i = 0; i < 500; ++i) {
            buffer.push(i);    // blocks when 64 items not yet consumed
        }
    });

    std::thread consumer([&] {
        int x;
        for (int n = 0; n < 500; ++n) {
            buffer.pop(x);     // blocks when empty
            if (n % 100 == 0)
                std::printf("consumed %d\n", x);
        }
    });

    producer.join();
    consumer.join();
    std::printf("done\n");
    return 0;
}
```

Non-blocking variant for event loops:

```cpp
if (!buffer.try_push(item)) {
    // buffer full — drop, spill to disk, or slow producer
}
```

**abort_push** attempts enqueue without blocking; if full, the item is
discarded — useful for **lossy** telemetry where fresh samples beat backlog.

---

## 3.5.4 Example: priority task queue

```cpp
// g++ -std=c++17 -O2 priority_queue_demo.cpp -ltbb
#include <oneapi/tbb/concurrent_priority_queue.h>
#include <cstdio>

struct Task {
    int priority;
    int id;
    bool operator<(const Task& o) const { return priority < o.priority; }
};

int main() {
    tbb::concurrent_priority_queue<Task> pq;
    pq.push({5, 1});
    pq.push({10, 2});
    pq.push({10, 3});   // same priority — order vs id undefined
    pq.push({1, 4});

    Task t;
    while (pq.try_pop(t))
        std::printf("pop id=%d pri=%d\n", t.id, t.priority);
    return 0;
}
```

**Trade-offs ▸**

| Container | Ordering | Bounds | Blocking | Typical use |
|-----------|----------|--------|----------|-------------|
| `concurrent_queue` | FIFO | unbounded | no | general MPMC |
| `concurrent_bounded_queue` | FIFO | yes | push/pop | back-pressure, pipeline |
| `concurrent_priority_queue` | by priority | unbounded | no (try only) | schedulers, best-first |

**Tuning ▸** Size the bounded queue to ~2–3× consumer batch latency × producer
rate — large enough to absorb jitter, small enough to cap memory. Pair with
[concurrent_vector](01-concurrent-vector.md) for result aggregation, not as a
substitute for queues.

---

## Summary

- **`concurrent_bounded_queue`**: **`set_capacity`**, blocking **`push`/`pop`**
  for back-pressure; **`try_push`/`try_pop`/`abort_push`** for non-blocking paths.
- **`concurrent_priority_queue`**: thread-safe heap; **`try_pop`** returns
  highest priority; **no ordering guarantee among equal priorities**.
- Use bounded queues as **work buffers** between pipeline stages; avoid
  blocking queue ops on hot TBB worker threads when possible.

Next: [4.1 — Mutexes: the TBB family](../04-synchronization/01-mutexes.md)
