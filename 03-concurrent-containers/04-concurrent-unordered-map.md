# 3.4 — concurrent_unordered_map

`concurrent_unordered_map<Key, T>` (and `concurrent_unordered_set<Key>`) is
the **STL-flavoured** concurrent hash table in oneTBB. The API feels like
`std::unordered_map`: `operator[]`, `insert`, `find`, range **`for` traversal**
— but **without** the accessor/lock objects that
[concurrent_hash_map](03-concurrent-hash-map.md) forces you to use.

That convenience has a cost: you get **references** into the table, not locked
views. Concurrent **insert** and **lookup** are safe; concurrent **erase is
not** supported safely; and **mutating a value through a reference** while
another thread touches the same key is **your bug**, not the container's.

---

## 3.4.1 STL-like surface, different concurrency contract

```
   concurrent_hash_map          concurrent_unordered_map
   ─────────────────────        ──────────────────────────
   accessor / const_accessor    operator[], find → reference
   explicit lock lifetime       no lock object — YOU infer safety
   concurrent erase ✓           concurrent erase ✗
   older, verbose               newer, ergonomic
```

> **The API ▸**
> ```cpp
> #include <oneapi/tbb/concurrent_unordered_map.h>
> #include <oneapi/tbb/concurrent_unordered_set.h>
>
> mapped_type& operator[](const key_type& k);  // insert default if missing
> iterator find(const key_type& k);
> std::pair<iterator, bool> insert(const value_type& v);
> size_type size() const;
> // erase: NOT safe concurrently with other operations
> ```
> **`concurrent_unordered_set<T>`** is the same machinery without mapped values —
> use for concurrent deduplication or symbol sets.

Traversal:

```cpp
for (const auto& kv : map)   // concurrent insert may invalidate iteration
    use(kv.first, kv.second);
```

Concurrent inserts during iteration are allowed by the API but **do not**
guarantee a consistent snapshot — you may see new entries or miss rehashed
layout mid-walk. Quiesce writers for a stable pass, or copy keys first.

---

## 3.4.2 Reference semantics: no value-mutation safety net

> **Pitfall ▸** This looks innocent and is **wrong** under concurrency:
> ```cpp
> map[key] += 1;   // ✗ read-modify-write on mapped_type is NOT atomic
> ```
> `operator[]` returns a reference to the value, but **incrementing** is load-
> add-store — another thread can interleave. Fix patterns:
> - Store **`std::atomic`** values (Part 4.4) if the mapped type is a scalar counter.
> - Use [concurrent_hash_map](03-concurrent-hash-map.md) **`accessor`** for
>   non-atomic complex values.
> - Use **`parallel_reduce`** (Part 1.2) instead of a shared map when possible.

> **Under the hood ▸** Fine-grained internal locking still exists — but locks
> are held only for the duration of each single map operation (`find`, `insert`,
> `operator[]`), not for the whole time you hold a reference. Anything you do
> **after** the function returns is unsynchronized unless you add your own mutex
> or atomic.

---

## 3.4.3 concurrent erase: don't

Unlike `concurrent_hash_map::erase`, **`erase` on concurrent_unordered_map is
not concurrency-safe** with other operations. Treat erase as **single-threaded
maintenance** (shutdown, GC phase) after all producers stop, or avoid erase
entirely (versioned tables, tombstone generations).

```
   ✓ concurrent insert (many threads)
   ✓ concurrent find / operator[] lookup
   ✓ concurrent traversal (weak snapshot semantics)
   ✗ concurrent erase
   ✗ unsynchronized RMW on mapped_type references
```

---

## 3.4.4 When to pick which map

| Need | Pick |
|------|------|
| Concurrent erase; locked read-modify-write on non-atomic values | `concurrent_hash_map` + accessor |
| Simple parallel insert + later read-only pass; atomic values | `concurrent_unordered_map` |
| Parallel word count with `std::atomic<int>` values | `concurrent_unordered_map` |
| Long-lived in-place updates to complex structs | `concurrent_hash_map` |
| Custom hash type with HashCompare trait | `concurrent_hash_map` (mature trait hook) |

**Trade-offs ▸**
- ✓ Ergonomic; good for insert-heavy parallel aggregation; set variant available.
- ✗ No safe concurrent erase; references mislead you into data races on values;
  iteration not a snapshot.

---

## 3.4.5 Example: parallel insert with atomic counts

```cpp
// g++ -std=c++17 -O2 concurrent_unordered_map_demo.cpp -ltbb
#include <oneapi/tbb/concurrent_unordered_map.h>
#include <oneapi/tbb/parallel_for.h>
#include <oneapi/tbb/blocked_range.h>
#include <atomic>
#include <string>
#include <vector>
#include <cstdio>

int main() {
    std::vector<std::string> tokens = {
        "one", "tbb", "one", "map", "tbb", "one", "concurrent"
    };

    // Atomic mapped values make += safe.
    tbb::concurrent_unordered_map<std::string, std::atomic<int>> freq;

    tbb::parallel_for(
        tbb::blocked_range<size_t>(0, tokens.size()),
        [&](const tbb::blocked_range<size_t>& r) {
            for (size_t i = r.begin(); i != r.end(); ++i) {
                freq[tokens[i]]++;   // atomic<int>::operator++ is safe
            }
        });

    for (const auto& kv : freq)
        std::printf("%s → %d\n", kv.first.c_str(), kv.second.load());
    return 0;
}
```

For non-atomic `int` values, use the accessor pattern in
[3.3 — concurrent_hash_map](03-concurrent-hash-map.md) instead.

**Tuning ▸** If profiling shows map contention, shard into N separate
`concurrent_unordered_map` instances keyed by `hash % N`, then merge — same
idea as reducing false sharing ([Part 5.2](../05-memory/02-cache-alignment-false-sharing.md)).

---

## Summary

- **`concurrent_unordered_map` / `concurrent_unordered_set`** expose an
  **`std::unordered_map`-like API** with concurrent insert and lookup.
- **No accessors** — references do **not** extend locks; value mutation needs
  **`std::atomic`** or external synchronization.
- **No concurrent erase** — unlike [concurrent_hash_map](03-concurrent-hash-map.md).
- Choose it for ergonomic insert-heavy parallelism; choose **`concurrent_hash_map`**
  when you need accessors, concurrent erase, or locked complex updates.

Next: [3.5 — Bounded & priority queues](05-bounded-and-priority-queues.md)
