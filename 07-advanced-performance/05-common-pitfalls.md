# 7.5 — Common Pitfalls

You know the scheduler (Part 0), the algorithms (Part 1), and the tuning loop (Part
7.4). This chapter collects **failure modes that still ship to production** — each
with the **mechanism** that bites you and the **fix** that matches how TBB actually
works.

---

## 7.5.1 Data races on shared state

**Mechanism:** Two tasks write the same non-atomic location without synchronization
→ undefined behavior. The work-stealing scheduler gives **no guarantee** about which
thread runs which iteration — "it worked in testing" is luck.

```cpp
// ✗ WRONG — race on shared counter
int count = 0;
tbb::parallel_for(0, N, [&](int i) {
    if (predicate(i)) ++count;  // UB: lost updates
});
```

**Fix:** Thread-local accumulation + one merge, `std::atomic`, concurrent container,
or reduction:

```cpp
// ✓ reduce, or atomic, or thread-local + merge
int count = tbb::parallel_reduce(
    tbb::blocked_range<int>(0, N), 0,
    [&](const tbb::blocked_range<int>& r, int local) {
        for (int i = r.begin(); i != r.end(); ++i)
            if (predicate(i)) ++local;
        return local;
    },
    std::plus<int>{});
```

See Part 4.4 (atomics) and Part 1.2 (`parallel_reduce`).

> **Pitfall ▸** "Read-only" shared data is safe only if **no writer** runs
> concurrently. A concurrent resize of `std::vector` while others read is still UB.

---

## 7.5.2 Too-fine-grained tasks

**Mechanism:** Body cost ≈ nanoseconds, task spawn ≈ tens of nanoseconds → deque
push/pop and steals dominate; more cores can run **slower** than one (Part 0.4).

```
   10M iterations, grain=1  →  ~10M tasks  →  scheduler meltdown
   10M iterations, grain=10k →  ~1k tasks   →  overhead ≪ work
```

**Fix:** Increase **grain size** until each task does ~1–10 µs of real work. Use
`blocked_range(0, N, grainsize)` or a heavier body per iteration.

---

## 7.5.3 Nested locks / accessor deadlock

**Mechanism:** `concurrent_hash_map` **accessors** are exclusive per bucket (Part
3.3). Thread A holds accessor on key *k1* and waits for *k2*; thread B holds *k2*
and waits for *k1* → deadlock. Same pattern with nested `mutex` locks in task bodies.

**Fix:** **Lock ordering** — always acquire in global key order. Prefer
`concurrent_unordered_map` + external sharded locks if accessor patterns are complex.
Never call user code that re-enters the map while holding an accessor.

---

## 7.5.4 Using std:: containers concurrently

**Mechanism:** `std::vector`, `std::unordered_map`, etc. are **not** thread-safe for
concurrent write or write-during-read. Reallocation invalidates iterators and data
pointers mid-parallel_for.

**Fix:** TBB concurrent containers (Part 3), preallocate before parallel phase, or
partition so each thread owns disjoint index ranges into separate buffers merged
after. `concurrent_vector` preserves `&v[i]` across growth; `std::vector` does not.

---

## 7.5.5 Blocking a worker on I/O

**Mechanism:** A worker blocked in `read()`, `sleep()`, or a heavy mutex wait **does
not** run tasks. With P workers, one blocked worker cuts capacity by 1/P; block all
workers → **pool starvation** — the whole parallel phase stalls.

```
   parallel_for body calls blocking HTTP
        │
        ▼
   workers parked in kernel  →  no steals, no progress, serial tail grows (Amdahl)
```

**Fix:** Offload I/O to a **separate thread pool** or async I/O; keep TBB workers
CPU-busy. Isolate blocking libraries in a **`task_arena(1)`** or non-TBB thread
(Part 2.2). Never `sleep` inside hot parallel bodies for "rate limiting."

---

## 7.5.6 False sharing

**Mechanism:** Independent atomics or counters on the **same cache line** (64 B)
→ cores invalidate each other's lines on every update → memory traffic scales with
cores, speedup inverts (Part 5.2).

```cpp
// ✗ counters[i] may share cache lines
std::atomic<int> counters[256];
tbb::parallel_for(0, 256, [&](int i) { counters[i]++; });
```

**Fix:** **`cache_aligned_allocator`**, padding to cache line, or
`enumerable_thread_specific` with one merge. Align hot per-thread accumulators.

---

## 7.5.7 Ignoring the subrange in parallel_for

**Mechanism:** The body receives **`r`** — the task's chunk. Looping `0..N` instead
of `r.begin()..r.end()` duplicates work and introduces races on writes.

```cpp
// ✗ processes entire array in every task
tbb::parallel_for(tbb::blocked_range<int>(0, N),
    [&](const tbb::blocked_range<int>& r) {
        for (int i = 0; i < N; ++i) a[i] *= 2;  // wrong
    });

// ✓ only this task's slice
tbb::parallel_for(tbb::blocked_range<int>(0, N),
    [&](const tbb::blocked_range<int>& r) {
        for (int i = r.begin(); i != r.end(); ++i) a[i] *= 2;
    });
```

Part 0.1 flagged this; it remains the #1 beginner bug in code review.

---

## 7.5.8 Assuming reduction order

**Mechanism:** `parallel_reduce` merge order follows the schedule → floating-point
results differ run to run (Part 7.3). Tests using `==` on `double` flake.

**Fix:** Epsilon compares in tests, integer/exact types when possible, or
**`parallel_deterministic_reduce`** when bit-identical reruns are required.

---

## 7.5.9 Capturing by reference to a dangling local

**Mechanism:** `parallel_for` returns after **blocking** until done — but captures
still must outlive the call. Fire-and-forget `task_group` without `wait()`, or
capturing stack locals in async continuations, uses **dangling references**.

```cpp
void bad() {
    tbb::task_group g;
    int local = 42;
    g.run([&] { use(local); });  // OK only if g.wait() before bad() returns
    // missing g.wait() → UB if group outlives stack frame
}
```

**Fix:** **`g.wait()`** before scope exit; capture by value for small POD; use
`std::shared_ptr` for shared ownership across async boundaries.

---

## 7.5.10 Do / don't quick reference

| Don't | Do instead |
|-------|------------|
| Share mutable state without sync | `parallel_reduce`, atomics, concurrent containers |
| grain=1 on cheap loop body | grainsize 1k–10k+; measure (Part 7.4) |
| Hold two hash_map accessors ad hoc | Fixed lock order; restructure |
| `std::vector` push_back in parallel | `concurrent_vector` or per-thread buffers |
| `read()` / long sleep in task body | Separate I/O threads; `task_arena` isolation |
| Adjacent atomic counters | Cache-line alignment (Part 5.2) |
| Loop `0..N` ignoring `r` | Loop `r.begin()..r.end()` |
| `assert(sum_parallel == sum_serial)` for `double` | Epsilon or deterministic reduce |
| `[&]` + async without lifetime plan | `wait()`, capture by value, `shared_ptr` |

```
   most production TBB bugs cluster here:
   ─────────────────────────────────────
   races · wrong range · too-fine tasks · blocking workers · false sharing
```

---

## Summary

- **Data races** on shared mutable state are UB — use reductions, atomics, or
  concurrent structures (Parts 1, 3, 4).
- **Too-fine tasks** waste the scheduler; **ignoring subrange `r`** duplicates work
  and races.
- **Accessor / nested lock** ordering prevents deadlock in concurrent_hash_map.
- **`std::` containers** are not parallel-safe for mutation; **blocking I/O** in
  workers starves the pool — isolate with separate threads or arenas.
- **False sharing** and **non-deterministic reduce order** need layout and API fixes
  (Parts 5.2, 7.3); **dangling captures** need explicit lifetime discipline.

Next: [API cheat sheet](../99-reference/api-cheatsheet.md)
