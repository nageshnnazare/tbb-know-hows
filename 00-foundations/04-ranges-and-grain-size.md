# 0.4 — Ranges, Splitting & Grain Size

Part 0.3 showed *how* workers exchange tasks. This chapter covers *what* gets
split: the **Range** concept, the recursive **split tree** that becomes the task
tree, and **grain size** — the stop rule that trades scheduling overhead against
load imbalance.

---

## 0.4.1 The Range concept

TBB parallel algorithms do not hard-code "loop from 0 to N." They operate on a
**Range** — a value type representing a chunk of work that can be subdivided:

```
   Required interface (conceptual):
   ─────────────────────────────────
   empty()           →  no work left?
   is_divisible()    →  worth splitting further?
   splitting ctor    →  Range(R& other, tbb::split)  // mutates other, takes half
   begin()/end()     →  iteration bounds (1D ranges)
   grainsize()       →  minimum chunk size (blocked_range family)
```

![Recursive range splitting produces a tree of subranges that map to scheduler tasks](figures/recursive-splitting.svg)

The algorithm repeatedly asks: *is this range divisible?* If yes, construct a
second range via the **split constructor**, enqueue one half as a task, continue
with the other. When `is_divisible()` returns false, the range is a **leaf** —
one body invocation runs it serially.

> **The API ▸** Standard 1D range:

```cpp
  #include <oneapi/tbb/blocked_range.h>
  tbb::blocked_range<size_t>(begin, end);              // default grainsize = 1
  tbb::blocked_range<size_t>(begin, end, grain_size);
```

> 2D / 3D: `<oneapi/tbb/blocked_range2d.h>`,
> `<oneapi/tbb/blocked_range3d.h>`. Semantics: half-open intervals `[begin, end)`
> in each dimension; split bisects the **largest** dimension that exceeds grain.

---

## 0.4.2 blocked_range mechanics

```cpp
// g++ -std=c++17 -O2 blocked_range_demo.cpp -ltbb -o blocked_range_demo
#include <oneapi/tbb/blocked_range.h>
#include <cstdio>

int main() {
    tbb::blocked_range<int> whole(0, 100, 10);  // [0,100), grain 10

    std::printf("whole: [%d,%d) size=%zu grain=%zu divisible=%d\n",
        whole.begin(), whole.end(), whole.size(), whole.grainsize(),
        whole.is_divisible());

    tbb::blocked_range<int> left(whole, tbb::split{});  // splits whole in half

    std::printf("left:  [%d,%d)\n", left.begin(), left.end());
    std::printf("right: [%d,%d)  (former 'whole')\n", whole.begin(), whole.end());
    return 0;
}
```

Typical output pattern:

```
   whole: [0,100) size=100 grain=10 divisible=1
   left:  [0,50)
   right: [50,100)
```

**Split constructor semantics:** `Range(R& r, split)` **mutates** `r` — it shrinks
`r` to the upper half and initializes `*this` with the lower half (exact half
depends on dimension for 2D/3D). Both halves inherit the same `grainsize()`.

```
   is_divisible()  ≡  (size() > grainsize())   // for blocked_range

   size() == grainsize()  →  leaf, one task, no further split
   size() <  grainsize()  →  still one leaf (undividable remainder)
```

> **Under the hood ▸** `parallel_for` wraps each leaf range in a **task** whose
> `execute()` calls your body. Internal splits create **continuation-style** tasks
> (Part 2.4) so the calling thread can keep working without waiting for children.

---

## 0.4.3 From split tree to task tree

Your range is the root of a binary split tree (per dimension policy for 2D):

```
   blocked_range[0, 1000), grain=100

              [0, 1000)
             /         \
        [0, 500)     [500, 1000)
        /      \        /       \
   [0,250) [250,500) ...     ...     ← stops when size ≤ 100
      │       │
    TASK     TASK                    ← each leaf → one body call
```

The **shape** of the tree depends on:

1. **Total work** (range size)
2. **Grain size** (minimum leaf)
3. **Partitioner** (Part 0.5) — whether all splits happen up front or lazily

The **scheduler** (Part 0.3) does not mirror the tree statically. Early splits
may sit in deques; thieves grab coarse nodes and split further on demand. The
range tree is the *logical* decomposition; the *physical* task set is dynamic.

---

## 0.4.4 What grain size actually controls

**Grain size** is **not** "number of threads" and **not** "iterations per thread."
It is the **smallest subrange the range type will agree to split**:

```
   grain too small  →  many tiny leaves  →  task spawn overhead dominates
   grain too large  →  few fat leaves    →  cores idle waiting for stragglers
   grain just right →  each leaf ≈ microseconds of useful work
```

![Grain size sets the leaf size of the split tree; too small adds overhead, too large causes imbalance](figures/grain-size.svg)

Think in **time**, not iteration count:

```
   work per iteration = w  (e.g. 50 ns for a[i]++, 500 ns for sqrt)
   target leaf time   ≈ 1–10 µs  (≈ 10k–100k cycles on modern CPUs)

   grain ≈ target_leaf_time / w
```

| Body cost | Rough grain starting point |
|-----------|----------------------------|
| ~10 ns (simple store/load) | 1'000 – 10'000 indices |
| ~100 ns (transcendental) | 100 – 1'000 |
| ~1 µs (small inner loop) | 10 – 100 |
| Irregular / unknown | default grain + **`auto_partitioner`** |

**Tuning ▸** Measure. Sweep grain over {g/4, g, 4g} with `std::chrono` or VTune.
The optimum is broad; within 2–4× of best is usually fine. Part 7.4 covers
systematic tuning and Amdahl limits.

---

## 0.4.5 The overhead vs imbalance trade-off

```
                    faster per core ↑
                         │
         too-small grain │    ╭── optimum band
         (overhead)      │   ╱
                         │  ╱
                         │ ╱
                         │╱
                         └──────────────────→ grain size
                              ╲
                               ╲  too-large grain
                                ╲ (imbalance)
```

**Overhead costs** (small grain):

- Task allocation and deque push/pop (Part 0.2)
- Splitting logic and partitioner decisions (Part 0.5)
- Worse cache reuse if chunks are smaller than a cache line's worth of work

**Imbalance costs** (large grain):

- One worker holds a fat leaf while others steal nothing useful
- Especially painful with **`static_partitioner`** (no steal recovery)

**Trade-offs ▸** Prefer **slightly too large** grain for perfectly uniform bodies;
prefer **smaller** grain (or `auto_partitioner`) when per-element cost varies.
`parallel_reduce` also pays merge cost at internal nodes — another reason not to
micro-split (Part 1.2).

---

## 0.4.6 Multi-dimensional ranges

For nested loops, express the iteration space as one range instead of nested
`parallel_for`:

```cpp
// g++ -std=c++17 -O2 range2d.cpp -ltbb -o range2d
#include <oneapi/tbb/parallel_for.h>
#include <oneapi/tbb/blocked_range2d.h>
#include <vector>
#include <cstdio>

int main() {
    const int R = 1024, C = 1024;
    std::vector<double> img(R * C, 1.0);

    tbb::parallel_for(
        tbb::blocked_range2d<int>(0, R, 0, C, 32),  // grain on shorter dim
        [&](const tbb::blocked_range2d<int>& br) {
            for (int i = br.rows().begin(); i != br.rows().end(); ++i)
                for (int j = br.cols().begin(); j != br.cols().end(); ++j)
                    img[i * C + j] *= 0.99;
        });

    std::printf("img[0]=%g\n", img.front());
    return 0;
}
```

`blocked_range2d` splits the **larger** extent first (unless below grain on that
axis). For matrix multiply, 2D blocking improves cache locality vs 1D row splits
(Part 1.1).

`blocked_range3d` adds `.pages()`, `.rows()`, `.cols()` for volumetric grids.

---

## 0.4.7 auto_partitioner adapts effective grain

With default **`auto_partitioner`** (Part 0.5), `grainsize()` is a **floor**, not
an exact leaf size: splits start coarse and refine when workers need work. Pairing
`blocked_range(0, n, 1)` with a cheap body still creates split pressure if the
partitioner subdivides eagerly — size grain by **time per iteration**, not index count.

> **Pitfall ▸** A grain of 1 with **`simple_partitioner`** on large N is almost
> always catastrophic (Part 0.5).

---

## 0.4.8 Custom ranges and a full loop

Any type with `empty()`, `is_divisible()`, a **split constructor**, and iteration
bounds works with `parallel_for` — useful for tree nodes, tiles, or bucket IDs.
The split tree mechanics are identical; only the bisection policy changes.

---

## 0.4.9 Putting it together

```cpp
// g++ -std=c++17 -O2 grain_tune.cpp -ltbb -o grain_tune
#include <oneapi/tbb/parallel_for.h>
#include <oneapi/tbb/blocked_range.h>
#include <vector>
#include <cmath>
#include <cstdio>

int main() {
    const size_t n = 20'000'000;
    std::vector<double> v(n, 2.0);

    constexpr size_t grain = 8'000;  // ~µs-scale chunks for this body

    tbb::parallel_for(
        tbb::blocked_range<size_t>(0, n, grain),
        [&](const tbb::blocked_range<size_t>& r) {
            for (size_t i = r.begin(); i != r.end(); ++i)
                v[i] = std::log1p(v[i]);
        });

    std::printf("v[0]=%g\n", v.front());
    return 0;
}
```

Checklist before you ship a `parallel_for`:

```
   ✓ body touches only [r.begin(), r.end())
   ✓ grain sized by time per iteration, not magic constants from a blog
   ✓ partitioner matches imbalance (0.5)
   ✓ no false sharing on writes (Part 5.2) — grain does not fix cache line fights
```

---

## Summary

- A **Range** is a splittable work chunk; **`blocked_range` / 2d / 3d** are the
  standard types with `[begin, end)` and **`grainsize()`**.
- The **split constructor** + **`is_divisible()`** build a **split tree** whose
  leaves become **tasks** on the work-stealing scheduler (Part 0.3).
- **Grain size** is the minimum leaf; tune for **~1–10 µs per chunk**, not a fixed
  iteration count.
- Too-small grain → **overhead**; too-large grain → **imbalance**; measure the band.
- **`auto_partitioner`** adapts effective chunk size above your grain floor (Part 0.5).

Next: [0.5 — Partitioners](05-partitioners.md)
