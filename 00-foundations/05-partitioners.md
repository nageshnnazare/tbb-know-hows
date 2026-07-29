# 0.5 — Partitioners

Part 0.4 defined **ranges** and **grain size** — the *what* and *when to stop
splitting*. A **partitioner** is the third knob: it controls *how aggressively*
the runtime subdivides a range and whether idle workers may **steal** work to fix
imbalance. Same range, same grain, different partitioner → different task tree and
different scaling behavior.

---

## 0.5.1 What a partitioner does

`parallel_for` (and `parallel_reduce`, `parallel_scan`) accepts an optional
**partitioner** argument after the body:

```
   parallel_for(range, body);                    // default: auto_partitioner
   parallel_for(range, body, tbb::simple_partitioner());
   parallel_for(range, body, tbb::static_partitioner());
   parallel_for(range, body, ap);                // affinity_partitioner& ap
```

![Partitioners trade load-balancing flexibility against splitting overhead and cache affinity](figures/partitioners.svg)

Mechanically, the partitioner answers questions the range alone cannot:

```
   Should we split this range now, or wait until a worker needs work?
   May another worker steal a piece of this range?
   Should we replay last call's thread-to-chunk mapping?
```

The partitioner works **with** `grainsize()` from the range (Part 0.4), not
instead of it.

> **The API ▸**
>
> ```cpp
> #include <oneapi/tbb/partitioner.h>
> tbb::auto_partitioner();
> tbb::simple_partitioner();
> tbb::static_partitioner();
> tbb::affinity_partitioner ap;   // reusable object; pass by reference
> ```
>
> Signature pattern:
> ```cpp
> template<typename Range, typename Body, typename Partitioner>
> void parallel_for(const Range& r, const Body& body, const Partitioner& p);
> ```
> Header: `<oneapi/tbb/parallel_for.h>`. Default partitioner is **`auto_partitioner`**.

---

## 0.5.2 auto_partitioner — default, adaptive

**Behavior:** Splits **lazily** and **adaptively**. Initial chunks are often **coarser**
than `grainsize()`; the runtime subdivides further when workers run out of local
work or imbalance is detected. **Work stealing enabled.**

```
   start with few large tasks
        │
        ├─ workers busy, balanced  →  minimal splitting, low overhead
        │
        └─ worker idle / steal     →  split coarse tasks, finer leaves appear
```

**When to use:** Almost always. Unknown or variable per-chunk cost, nested
parallelism, first pass before tuning.

**Trade-offs ▸**

| ✓ | ✗ |
|---|---|
| Adapts to imbalance via stealing (Part 0.3) | Slightly higher scheduling logic than static |
| Forgiving grain size | Task count varies run-to-run |
| Default in `parallel_for` | Not the best for strict repeatability |

> **Under the hood ▸** `auto_partitioner` implements **auto chunking** (historically
> related to the "auto" grainsize idea in older TBB docs). Your `blocked_range`
> `grainsize()` is a **minimum** leaf size; auto may keep chunks larger until
> splitting pays off.

---

## 0.5.3 simple_partitioner — split to grainsize, always

**Behavior:** Eagerly splits the range until each piece is **at most `grainsize()`**
(assuming the range's `is_divisible()` agrees). Still uses the work-stealing
scheduler — stolen tasks are already near-leaf granularity.

```
   [0, 10000), grain=100  →  ~100 leaf tasks created up front (conceptually)
```

**When to use:**

- Uniform cost per iteration **and** you have tuned grain precisely
- You want predictable leaf count for reasoning about overhead
- Slightly lower partitioner logic than auto on **uniform** loops

**Trade-offs ▸**

| ✓ | ✗ |
|---|---|
| Predictable decomposition to grain | No adaptation above grain — bad if cost varies |
| Good with well-measured grain | Eager splits → more tasks than auto on large ranges |
| Stealing still helps stragglers | Worse than auto when grain is wrong |

> **Pitfall ▸** Pairing `simple_partitioner` with a **too-small** grain on a cheap
> body recreates the "million tasks" failure mode (Part 0.2). Measure leaf **time**,
> not just iteration count.

---

## 0.5.4 static_partitioner — one chunk per worker, no stealing

**Behavior:** Partitions the range into **approximately P contiguous slices** where
P is the number of workers in the current arena — **one chunk per worker**, sized
`total / P`. **Disables work stealing** for this `parallel_for`.

```
   8 workers, [0, 8000)  →  W0:[0,1000) W1:[1000,2000) ... W7:[7000,8000)
                           fixed for entire invocation
```

**When to use:**

- **Uniform** work per element, stable across the domain
- You want **deterministic** assignment for debugging or reproducible benchmarks
- **Cache locality** from each worker walking a contiguous index block (Part 7.4)

**Trade-offs ▸**

| ✓ | ✗ |
|---|---|
| Minimal partition overhead | **No steal recovery** — stragglers leave cores idle |
| Deterministic thread ↔ index mapping | Catastrophic on imbalanced bodies |
| Excellent for uniform `a[i] = f(a[i])` | Wrong if per-index cost varies |

> **Under the hood ▸** Because stealing is off, the spawn tree is **flat**: P tasks,
> not a deep split tree. This interacts well with **`blocked_range`** grain set
> large enough that each static slice is a single leaf — but if grain forces
> multiple leaves per worker, you still get several tasks per worker, just no
> cross-worker steals.

---

## 0.5.5 affinity_partitioner — replay mapping for cache reuse

**Behavior:** Like **`auto_partitioner`** for splitting and stealing on the **first**
invocation, but records **which subranges ran on which worker**. On **subsequent**
calls with the **same partitioner object** and **same range structure**, it tries to
assign the same chunks to the same workers.

```
   iteration 1:  W2 got [2000,3000)  →  partitioner remembers
   iteration 2:  W2 prefers [2000,3000) again  →  data may still be in cache
```

**When to use:**

- **Repeated** `parallel_for` over the **same data** (time-stepping, iterative
  refinement, multi-pass filters)
- Range bounds and grain unchanged call-to-call
- CPU-bound loops where cache reuse matters (Part 5.2, Part 7.4)

**Requirements:**

```
   ✓ reuse the SAME affinity_partitioner instance
   ✓ same range dimensions / grain across calls
   ✗ do not expect affinity if worker count changes (global_control, different arena)
```

**Trade-offs ▸**

| ✓ | ✗ |
|---|---|
| Can improve L2/L3 hit rate across passes | Stateful — must manage object lifetime |
| Still adaptive on first pass | Benefit ≈ 0 if data doesn't fit cache or pattern changes |
| Good for Jacobi-style stencils | Wrong tool for one-shot loops |

> **Pitfall ▸** Creating a **new** `affinity_partitioner` every iteration resets the
> map — you pay first-pass behavior forever. Declare it **outside** the hot loop.

---

## 0.5.6 Decision guide

| Situation | Pick |
|-----------|------|
| Default / unknown / variable cost | `auto_partitioner` |
| Uniform + tuned grain | `simple_partitioner` |
| Uniform + deterministic contiguous slices | `static_partitioner` |
| Same loop over same data, many passes | `affinity_partitioner` (reuse object) |

Part 1.1 applies these to real `parallel_for` bodies; Part 7.5 lists partitioner pitfalls.

---

## 0.5.7 Partitioner × grain size interaction

Grain and partitioner multiply:

```
   auto  + large grain   →  few tasks initially; splits on demand
   auto  + small grain   →  finer minimum when splits happen
   simple + grain G      →  always push toward leaves of size ≤ G
   static + any grain    →  P slices; grain only matters if slice > grain
```

Example: 10⁷ iterations, 16 workers, body ~100 ns/iter.

| Config | Effect |
|--------|--------|
| `blocked_range(0,n,5000)` + `auto_partitioner()` | Safe default; ~2000 min-index leaves possible, often fewer live |
| `blocked_range(0,n,5000)` + `simple_partitioner()` | ~2000 leaves eagerly; uniform work OK |
| `blocked_range(0,n,5000)` + `static_partitioner()` | 16 contiguous mega-chunks; great if uniform, fatal if `cost(i)` varies |

**Tuning ▸** Tune **grain first** with `auto_partitioner`. Switch to `static` or
`affinity` only after profiling shows stable uniform cost or repeated-pass cache wins.

---

## 0.5.8 Complete example — all four partitioners

```cpp
// g++ -std=c++17 -O2 partitioners.cpp -ltbb -o partitioners
#include <oneapi/tbb/parallel_for.h>
#include <oneapi/tbb/blocked_range.h>
#include <oneapi/tbb/partitioner.h>
#include <vector>
#include <cmath>
#include <cstdio>

int main() {
    const int n = 10'000'000;
    std::vector<double> data(n, 1.0);
    const tbb::blocked_range<int> range(0, n, 8'000);

    auto body = [&](const tbb::blocked_range<int>& r) {
        for (int i = r.begin(); i != r.end(); ++i)
            data[i] = std::sqrt(data[i]) + 1.0;
    };

    // 1) Default — adaptive splitting + stealing
    tbb::parallel_for(range, body);

    // 2) Eager split to grainsize
    tbb::parallel_for(range, body, tbb::simple_partitioner());

    // 3) Fixed slice per worker, no steal
    tbb::parallel_for(range, body, tbb::static_partitioner());

    // 4) Repeated passes — reuse affinity map
    tbb::affinity_partitioner ap;
    for (int pass = 0; pass < 3; ++pass)
        tbb::parallel_for(range, body, ap);

    std::printf("data[0]=%g\n", data.front());
    return 0;
}
```

For **`parallel_reduce`** and **`parallel_scan`**, pass the partitioner as the last
argument before any optional arena (Part 1.2, Part 1.3) — same semantics.

---

## 0.5.9 Relation to the rest of the guide

Part 0 is complete: **tasks** (0.2) on **work-stealing deques** (0.3), splitting
**ranges** to a **grain floor** (0.4), under a **partitioner policy** (0.5). Part 1
applies this to algorithms; Part 7 covers measurement (idle cores with
`static_partitioner` → try `auto`; steal storms → increase grain).

> **Under the hood ▸** Partitioners are policy objects (by value, except reusable
> **`affinity_partitioner&`**) consulted while the algorithm template walks the
> range split tree.

---

## Summary

- **Partitioners** control splitting aggressiveness and whether **work stealing**
  applies; they complement **grain size** from the range (Part 0.4).
- **`auto_partitioner`** (default): lazy, adaptive splits + stealing — use unless you
  know otherwise.
- **`simple_partitioner`**: eager splits down to **grainsize** — uniform work with
  tuned grain.
- **`static_partitioner`**: **one chunk per worker**, **no stealing** — uniform,
  deterministic, cache-friendly; fails on imbalance.
- **`affinity_partitioner`**: remembers worker ↔ chunk mapping across **repeated**
  calls — reuse the same object in hot loops.

Next: [Part 1.1 — parallel_for](../01-parallel-algorithms/01-parallel-for.md)
