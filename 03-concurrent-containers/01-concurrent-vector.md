# 3.1 — concurrent_vector

`concurrent_vector<T>` is TBB's answer to a question `std::vector` cannot
safely handle: **multiple threads appending and indexing concurrently** without
you wrapping every `push_back` in a mutex. Mechanically it is a **segmented
dynamic array** — not one contiguous slab — so element **addresses stay valid
across growth**, unlike `std::vector` which reallocates and invalidates
`&v[i]`.

That stability property is the reason it exists. Everything else — slightly
slower indexing, no concurrent erase/insert-in-middle — follows from the
segmented layout.

---

## 3.1.1 Segmented storage vs std::vector

![concurrent_vector segmented layout with stable element addresses](figures/concurrent-vector.svg)

```
   std::vector                         concurrent_vector
   ─────────────                       ─────────────────

   [ e0 | e1 | e2 | e3 | ... ]         segment 0: [ e0  e1  ... eK-1 ]
   ▲ contiguous                        segment 1: [ eK  eK+1 ... e2K-1 ]
   ▲ realloc → ALL pointers die        segment 2: [ ...              ]
                                       ▲ NOT contiguous across segments
                                       ▲ grow → NEW segment; old &e[i] STILL valid
```

When `std::vector` outgrows its capacity it allocates a bigger block, copies
(or moves) every element, and frees the old block. Any pointer, reference, or
iterator into the old storage is **undefined** after that. In parallel code
that is catastrophic: thread A holds `&results[i]` while thread B's
`push_back` triggers reallocation.

`concurrent_vector` never moves an element once its segment is allocated.
Growth allocates a **new segment** and links it into an index table. Existing
elements stay put. The trade-off: `operator[]` does an extra indirection
(segment lookup + offset) and the container is **not contiguous**, so
cache-friendly sequential scans are slightly slower than `std::vector`.

> **Under the hood ▸** Internally the container maintains a table of segment
> pointers. `size()` is atomic; `push_back` / `grow_by` may allocate a new
> segment under a short internal lock, but **per-element access** to already-
> allocated slots is lock-free for plain types. Concurrent growth from many
> threads is supported; the runtime serializes segment allocation, not every
> append.

---

## 3.1.2 Growth API: push_back, grow_by, grow_to_at_least

> **The API ▸**
> ```cpp
> #include <oneapi/tbb/concurrent_vector.h>
>
> void push_back(const T& value);          // append one; may allocate segment
> void push_back(T&& value);
> iterator grow_by(size_type delta);       // atomically extend by delta slots
> size_type grow_to_at_least(size_type n); // ensure size() >= n; return old size
> reference operator[](size_type i);       // concurrent R/W on slot i (if exists)
> size_type size() const;                 // current element count
> ```
> `grow_by(n)` returns an **iterator** to the first new slot (index =
> `size() - n` after the call). `grow_to_at_least(n)` returns the **previous**
> size — use it as the starting index for initializing new elements.
> `push_back` does not return an index; use `size() - 1` after push if you
> need the slot (or prefer `grow_by(1)` and write through the iterator).

The growth operations are the heart of the container. They extend the logical
size atomically so multiple threads can reserve distinct index ranges without
a surrounding mutex:

```
   thread 0: grow_by(100) → indices [0, 99)
   thread 1: grow_by(100) → indices [100, 199)   (no overlap)
   thread 2: push_back(x) → index 200
```

After growth, each thread owns its slice and can write `vec[i]` concurrently
with others writing different `i`. **Do not** have two threads write the same
index without your own synchronization.

> **Pitfall ▸** `grow_by` / `grow_to_at_least` **reserve** slots; they do not
> default-construct non-trivial `T` in all configurations the way
> `std::vector::resize` does. For `int` and other trivial types you write
> values explicitly. For non-trivial types, consult the oneTBB docs for your
> version — initialization semantics matter.

---

## 3.1.3 What you cannot do concurrently

```
   ✓ push_back / grow_by / grow_to_at_least  (from many threads)
   ✓ operator[] read/write on EXISTING indices (distinct indices)
   ✓ size() read

   ✗ erase / insert in the middle
   ✗ shrink / clear while others access
   ✗ assume iteration sees a stable snapshot while others grow
   ✗ pass &vec[i] to code that assumes contiguous layout (SIMD, pointer arithmetic)
```

There is no concurrent `erase`. If you need removal, use a different structure
or a two-phase "mark then compact" pattern outside the hot path. Iterators
can be invalidated by **growth** (new segments), but **element addresses**
(`&vec[i]` for fixed `i`) remain valid once the slot exists.

---

## 3.1.4 Example: parallel_for filling a concurrent_vector

Each task computes a result and appends it. No mutex — growth handles
concurrency.

```cpp
// g++ -std=c++17 -O2 concurrent_vector_demo.cpp -ltbb
#include <oneapi/tbb/concurrent_vector.h>
#include <oneapi/tbb/parallel_for.h>
#include <oneapi/tbb/blocked_range.h>
#include <cstdio>
#include <cmath>

struct Particle { double x, y, z; };

int main() {
    tbb::concurrent_vector<Particle> hits;

    // Each iteration appends one hit — safe without a lock.
    tbb::parallel_for(
        tbb::blocked_range<int>(0, 1'000'000),
        [&](const tbb::blocked_range<int>& r) {
            for (int i = r.begin(); i != r.end(); ++i) {
                hits.push_back({std::sin(i), std::cos(i), i * 0.001});
            }
        });

    std::printf("hits=%zu first=(%g,%g) last=(%g,%g)\n",
                hits.size(),
                hits.front().x, hits.front().y,
                hits.back().x, hits.back().y);
    return 0;
}
```

Alternative when each task produces a **known batch** — reserve with
`grow_by`, then fill by index (often faster than many `push_back` calls):

```cpp
tbb::parallel_for(
    tbb::blocked_range<size_t>(0, num_tasks),
    [&](const tbb::blocked_range<size_t>& r) {
        for (size_t t = r.begin(); t != r.end(); ++t) {
            auto it = hits.grow_by(batch_size);   // iterator to first new slot
            for (size_t j = 0; j < batch_size; ++j, ++it)
                *it = compute(t, j);
        }
    });
```

> **Pitfall ▸** Do not capture a pointer from `push_back` in one thread and
> assume another thread's growth cannot affect **iteration** over the whole
> container. Element stability is per-index, not "the whole vector is frozen."

---

## 3.1.5 When to use concurrent_vector vs std::vector

| Situation | Pick |
|-----------|------|
| Single-threaded, need max speed & contiguity | `std::vector` |
| Parallel **append** or parallel **index write** into growing array | `concurrent_vector` |
| Need stable `&elem` across growth | `concurrent_vector` |
| Need `data()` for SIMD / zero-copy I/O on whole array | `std::vector` (contiguous) |
| Known size upfront, parallel fill of fixed indices | pre-sized `std::vector` or `grow_to_at_least` once then `operator[]` |

**Trade-offs ▸**
- ✓ Lock-free-ish growth; stable element addresses; composes with
  `parallel_for` (Part 1.1).
- ✗ Extra indirection on `operator[]`; not contiguous; no concurrent erase;
  sequential single-thread performance loses to `std::vector`.

**Tuning ▸** Prefer `grow_by(batch)` over per-element `push_back` when each
task produces many items — fewer segment allocations. If false sharing on
adjacent writes hurts scaling, see [Part 5.2 — Cache alignment & false
sharing](../05-memory/02-cache-alignment-false-sharing.md): pad hot per-thread
records or give each task its own index range (which `grow_by` naturally does).

If you only need a thread-safe **queue** (FIFO), not random access, use
[concurrent_queue](02-concurrent-queue.md) instead.

---

## Summary

- `concurrent_vector` uses **segmented storage** so `&v[i]` stays valid when
  other threads grow the container — the opposite of `std::vector` reallocation.
- **`push_back`**, **`grow_by`**, and **`grow_to_at_least`** extend size
  concurrently; `grow_by` returns an iterator, `grow_to_at_least` returns the
  old size as a starting index.
- Indexing is concurrent on distinct slots, but there is **no concurrent
  erase/insert** and the layout is **not contiguous**.
- Prefer it for parallel append/fill patterns with `parallel_for`; prefer
  `std::vector` for single-threaded contiguity and peak sequential speed.

Next: [3.2 — concurrent_queue](02-concurrent-queue.md)
