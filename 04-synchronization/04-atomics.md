# 4.4 — Atomics

For **tiny shared scalars** — counters, flags, pointer heads in lock-free
structures — **`std::atomic`** beats any TBB mutex: no lock object, no waiter
queue, hardware atomic instructions on the word itself. oneTBB historically
shipped `tbb::atomic`; modern oneTBB **deprecated it in favour of
`std::atomic`** from `<atomic>`. New code uses the standard type exclusively.

Atomics are the building blocks of lock-free algorithms; the workhorse
operation is **compare-and-swap (CAS)** and the **retry loop** built on it.

---

## 4.4.1 Prefer std::atomic

> **The API ▸**
> ```cpp
> #include <atomic>
> std::atomic<int> counter{0};
> counter.load();                    // read
> counter.store(42);                 // write
> counter.fetch_add(1);              // post-increment; returns old value
> counter.exchange(new_val);         // swap; returns old
> ```
> **`tbb::atomic`** — do not use in new code. Interop with TBB containers that
> expect atomics (e.g. `concurrent_unordered_map<K, std::atomic<int>>`, Part 3.4)
> uses **`std::atomic`**.

> **Under the hood ▸** The compiler lowers atomics to CPU instructions (`LOCK
> ADD`, `XCHG`, `CMPXCHG` on x86). No kernel unless you use **`wait`/`notify`**
> (C++20). Still subject to **cache-line contention** when many cores hammer
> one atomic — see [Part 5.2](../05-memory/02-cache-alignment-false-sharing.md):
> pad or shard counters.

---

## 4.4.2 Compare-and-swap and the retry loop

![compare_exchange loop: load expected, CAS, retry on failure](figures/atomic-cas.svg)

```
   expected = atomic.load()
   loop:
       desired = f(expected)
       if CAS(expected, desired) success → done
       else → expected updated to current; retry
```

> **The API ▸**
> ```cpp
> std::atomic<int> v{0};
> int expected = 0;
> int desired = 1;
> bool ok = v.compare_exchange_weak(expected, desired);
> // weak: may spuriously fail — use in loops
> ok = v.compare_exchange_strong(expected, desired);
> // strong: no spurious failure — use outside tight loops
> ```
> On failure, **`expected` is overwritten** with the current value — that's
> how you retry with fresh state.

Lock-free stack push (illustrative):

```cpp
void push(Node* n) {
    n->next = head.load(std::memory_order_relaxed);
    while (!head.compare_exchange_weak(n->next, n,
            std::memory_order_release, std::memory_order_relaxed)) {
        // n->next refreshed from failed CAS; retry
    }
}
```

Every lock-free structure you have seen — queues, freelists, epoch reclamation —
is variations on **load → compute → CAS → retry**.

---

## 4.4.3 Memory order (intuition)

Default **`memory_order_seq_cst`** is safest and costliest — total global order.
For hot paths, relax when you can prove visibility:

| Order | Intuition |
|-------|-----------|
| `relaxed` | Atomicity only; no ordering vs other locations |
| `acquire` | **Load** — see prior releases on this atomic |
| `release` | **Store** — prior writes visible to acquirers |
| `acq_rel` | RMW both sides |
| `seq_cst` | Sequential consistency — default |

Typical publish pattern:

```cpp
data.ready.store(true, std::memory_order_release);   // producer
while (!data.ready.load(std::memory_order_acquire))  // consumer
    ;
// safe to read payload written before release
```

> **Pitfall ▸** Using **`relaxed`** everywhere for "speed" without a happens-
> before story is a subtle bug factory. Start with defaults; tighten only with
> a documented ordering argument (and tests under ThreadSanitizer).

---

## 4.4.4 When atomics beat mutexes — and when they don't

```
   atomics win:
   ✓ single counter, flag, pointer head
   ✓ fetch_add / CAS retry loops
   ✓ per-core sharded atomics merged once (avoids one hot line)

   atomics lose:
   ✗ protecting std::vector, std::map, multi-field invariants
   ✗ "lock" around I/O or long computation
   ✗ complex structures → use concurrent containers (Part 3) or mutex (Part 4.1)
```

A single **`std::atomic<int>`** counter can still **saturate one cache line**
under extreme contention — then use **`parallel_reduce`** (Part 1.2) or
**thread-local accumulators** ([Part 5.3](../05-memory/03-thread-local-storage.md))
and merge once.

---

## 4.4.5 Example: fetch_add vs mutex vs CAS max

```cpp
// g++ -std=c++17 -O2 atomics_demo.cpp -ltbb
#include <atomic>
#include <oneapi/tbb/mutex.h>
#include <oneapi/tbb/parallel_for.h>
#include <oneapi/tbb/blocked_range.h>
#include <cstdio>

int main() {
    std::atomic<int> sum{0};
    std::atomic<int> max_val{0};
    tbb::mutex max_mtx;
    int max_locked = 0;

    tbb::parallel_for(
        tbb::blocked_range<int>(0, 1'000'000),
        [&](const tbb::blocked_range<int>& r) {
            for (int i = r.begin(); i != r.end(); ++i) {
                sum.fetch_add(1, std::memory_order_relaxed);

                // CAS retry for running maximum
                int cur = max_val.load(std::memory_order_relaxed);
                while (i > cur && !max_val.compare_exchange_weak(
                           cur, i, std::memory_order_relaxed)) {}

                // mutex path for comparison (slower under contention)
                {
                    tbb::mutex::scoped_lock lk(max_mtx);
                    if (i > max_locked) max_locked = i;
                }
            }
        });

    std::printf("sum=%d max=%d max_locked=%d\n",
                sum.load(), max_val.load(), max_locked);
    return 0;
}
```

For **`max`**, **`parallel_reduce`** is often cleaner than a CAS loop — atomics
shine when the update is **one instruction** (`fetch_add`, `exchange`).

**Trade-offs ▸**
- ✓ Lowest latency for scalars; basis of lock-free designs.
- ✗ No compound invariants; easy to get memory order wrong; hot single word
  still serializes cache line.

**Tuning ▸** Shard: `std::array<std::atomic<int>, N>` with thread id hash;
align to cache lines. Read [Part 5.2](../05-memory/02-cache-alignment-false-sharing.md)
before micro-optimizing memory orders.

---

## Summary

- Use **`std::atomic`**, not deprecated **`tbb::atomic`**, for lock-free scalars.
- Core ops: **load/store**, **fetch_add**, **exchange**,
  **compare_exchange_weak/strong**; CAS **retry loops** underpin lock-free code.
- **Memory order**: default `seq_cst`; **release/acquire** for publish/consume;
  don't relax without proof.
- Atomics for **tiny** state; mutexes or **concurrent containers** for
  **complex** shared structures.

Next: [Part 5.1 — The scalable allocator](../05-memory/01-scalable-allocator.md)
