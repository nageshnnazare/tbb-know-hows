# 3.3 — concurrent_hash_map

`concurrent_hash_map<Key, T>` is a thread-safe hash table with **fine-grained
locking**: each bucket (or small group of buckets) has its own lock, so
unrelated keys can be inserted, found, and erased **concurrently**. Safe access
to a key's value goes through **`accessor`** (exclusive write) or
**`const_accessor`** (shared read) — RAII holders that acquire the bucket
lock on construction and release it on destruction.

This is the container to reach for when you need **concurrent insert, find,
erase, and in-place value mutation** with explicit lock lifetime — unlike
[concurrent_unordered_map](04-concurrent-unordered-map.md), which exposes
references without accessor semantics.

---

## 3.3.1 Accessors and bucket locks

![concurrent_hash_map bucket locking with accessor and const_accessor](figures/concurrent-hash-map.svg)

```
   key "apple"  ──hash──▶  bucket 3  ──[ lock ]──▶  ("apple", 5)
   key "banana" ──hash──▶  bucket 7  ──[ lock ]──▶  ("banana", 3)
                              ▲
                              └── different buckets → concurrent ops OK
```

> **The API ▸**
> ```cpp
> #include <oneapi/tbb/concurrent_hash_map.h>
>
> class accessor;        // exclusive: read/write value; one writer per bucket
> class const_accessor;  // shared: read-only; multiple const_accessors OK
>
> bool insert(accessor& a, const Key& key);  // true if new key inserted
> bool find(accessor& a, const Key& key);      // true if found; locks bucket
> bool find(const_accessor& ca, const Key& key);
> bool erase(const Key& key);
> size_type size() const;
> ```
> While an **`accessor`** lives, you hold an **exclusive** lock on that key's
> bucket: `a->second = 42` is safe. While a **`const_accessor`** lives, you
> hold a **shared** lock: read `ca->second`, but do not mutate. When the
> accessor object goes **out of scope**, the lock is **released automatically**
> (RAII).

Typical read-modify-write:

```cpp
tbb::concurrent_hash_map<std::string, int> map;

{
    tbb::concurrent_hash_map<std::string, int>::accessor a;
    if (map.find(a, "count")) {
        a->second += 1;           // locked update
    }
}   // lock released here — other threads can touch "count"
```

Insert-or-update:

```cpp
tbb::concurrent_hash_map<std::string, int>::accessor a;
if (map.insert(a, word)) {
    a->second = 1;                // new key — initialize
} else {
    a->second += 1;               // existed — increment under lock
}
```

> **Under the hood ▸** The table is an array of buckets, each with a reader-
> writer or mutex lock. `find`/`insert` hash the key, lock **one** bucket, then
> traverse the bucket's chain. Rehashing can relocate entries but **does not
> invalidate accessors** already holding a node — the accessor keeps the node
> pinned until release.

---

## 3.3.2 HashCompare trait

> **The API ▸**
> ```cpp
> template <typename Key, typename T,
>           typename HashCompare = tbb::tbb_hash_compare<Key>>
> class concurrent_hash_map;
> ```
> Custom hash/equality for non-standard keys:
> ```cpp
> struct MyHashCompare {
>     size_t hash(const MyKey& k) const { /* ... */ }
>     bool equal(const MyKey& a, const MyKey& b) const { return a == b; }
> };
> tbb::concurrent_hash_map<MyKey, Val, MyHashCompare> map;
> ```
> **`hash` and `equal` must be consistent** — equal keys must hash the same.
> Keep them pure and fast; they run on every access under lock.

---

## 3.3.3 The two-accessor deadlock pitfall

> **Pitfall ▸** **Never hold two accessors at once** (especially on different
> keys in different buckets) if another thread can acquire them in the **reverse
> order**:
> ```cpp
> // Thread 1                          // Thread 2
> accessor a1, a2;
> map.find(a1, "A");                   map.find(a2, "B");
> map.find(a2, "B");  // waits for B    map.find(a1, "A");  // waits for A
>                                      // → classic deadlock
> ```
> Rule: **one accessor, one scope, one key operation, then release.** Need two
> keys? Copy values out, release, then re-lock in a **global key order**, or
> use a coarser lock, or restructure to one-key updates.

Same-thread nested `find` on the **same** key without releasing the first
accessor is also wrong — the bucket lock is not recursive.

---

## 3.3.4 Example: parallel word count

```cpp
// g++ -std=c++17 -O2 concurrent_hash_map_demo.cpp -ltbb
#include <oneapi/tbb/concurrent_hash_map.h>
#include <oneapi/tbb/parallel_for.h>
#include <oneapi/tbb/blocked_range.h>
#include <string>
#include <vector>
#include <cstdio>

int main() {
    std::vector<std::string> words = {
        "tbb", "map", "tbb", "hash", "map", "tbb", "concurrent"
    };
    tbb::concurrent_hash_map<std::string, int> counts;

    tbb::parallel_for(
        tbb::blocked_range<size_t>(0, words.size()),
        [&](const tbb::blocked_range<size_t>& r) {
            for (size_t i = r.begin(); i != r.end(); ++i) {
                tbb::concurrent_hash_map<std::string, int>::accessor a;
                if (counts.insert(a, words[i]))
                    a->second = 1;
                else
                    a->second += 1;
            }
        });

    tbb::concurrent_hash_map<std::string, int>::const_accessor ca;
    if (counts.find(ca, "tbb"))
        std::printf("tbb count=%d map size=%zu\n", ca->second, counts.size());
    return 0;
}
```

Read-heavy caches: prefer **`const_accessor`** in `find` so multiple threads
can read distinct keys (or the same key) concurrently when the bucket lock
supports shared mode.

**Trade-offs ▸**
- ✓ Concurrent insert/find/erase; explicit lock scope; `const_accessor` for
  shared reads.
- ✗ Verbose accessor API; two-accessor deadlock risk; iterator walks are not
  a snapshot under concurrent mutation.

**Tuning ▸** Choose a good hash to spread buckets — hot buckets serialize
threads. For a simpler STL-like API without accessors (but weaker erase
guarantees), see [concurrent_unordered_map](04-concurrent-unordered-map.md).

Prefer [concurrent_vector](01-concurrent-vector.md) or reductions (Part 1.2)
when the problem is append-only, not keyed lookup.

---

## Summary

- **`accessor`** = exclusive bucket lock for read/write; **`const_accessor`** =
  shared read lock — both release on scope exit (RAII).
- Use **`insert`/`find`/`erase`** through accessors; one accessor per scope
  avoids **deadlock**.
- Customize hashing with **`HashCompare`** (`hash` + `equal`).
- Best for concurrent maps needing **safe in-place updates** and **concurrent
  erase**; compare with [concurrent_unordered_map](04-concurrent-unordered-map.md)
  for API style trade-offs.

Next: [3.4 — concurrent_unordered_map](04-concurrent-unordered-map.md)
