# 7.3 — Deterministic Reduction

Part 1.2 introduced `parallel_reduce` — split subranges, partial accumulators, tree
combine. For integer sums the result is usually identical run to run. For
**floating-point** (and some user-defined combines), **`parallel_reduce` is not
bit-reproducible**: the split tree varies with scheduling, so addition order changes
and rounding differs. This chapter explains why, and when to pay the cost of
**`parallel_deterministic_reduce`**.

---

## 7.3.1 Why parallel_reduce is non-deterministic

`parallel_reduce` uses the same adaptive splitting as `parallel_for` (Part 0.4):
ranges divide until grain size, thieves steal coarse chunks, load balance shifts
with timing. The **combine tree** — which partial result merges with which — is
therefore **schedule-dependent**.

```
   run A ( thief steals early )          run B ( different steal order )

        partials:  [a] [b] [c] [d]              [a] [b] [c] [d]
                      \   /   \   /                 \     /   /
                       \ /     \ /                   \   /   /
                     ((a+b)+(c+d))                 ((a+b+c)+d)
                           │                              │
                     different FP rounding          different FP rounding
```

Floating-point addition is **not associative**: `(a + b) + c ≠ a + (b + c)` in
general. Integer addition in two's complement **is** associative until overflow —
but even then, schedule variance can change *which* overflow occurs.

> **Under the hood ▸** `auto_partitioner` (Part 0.5) optimizes for throughput:
> splits follow demand, not a fixed plan. That is the right default for performance
> and the root cause of non-repeatable reduction order.

---

## 7.3.2 parallel_deterministic_reduce — fixed split tree

`parallel_deterministic_reduce` accepts the same body and combine functors as
`parallel_reduce`, but forces a **reproducible decomposition**. With
`simple_partitioner` or `static_partitioner`, the split pattern depends only on
range size and grain — **not** on steal order — so the combine tree is identical
every run.

```
   deterministic split (simple_partitioner, fixed grainsize)
   ─────────────────────────────────────────────────────────
   [0 ─────────────── N)
        ├─ [0 ── N/2)     ─┐
        └─ [N/2 ── N)     ─┤ same tree every run
              ├─ …        ─┘
              └─ …
```

> **The API ▸**
> ```cpp
> #include <oneapi/tbb/parallel_reduce.h>
>
> template<typename Range, typename Value, typename Body, typename Combine>
> Value parallel_deterministic_reduce(
>     const Range& range,
>     Value identity,
>     const Body& body,
>     const Combine& combine);
>
> // overload with explicit partitioner (simple_partitioner or static_partitioner)
> template<typename Range, typename Value, typename Body, typename Combine,
>          typename Partitioner>
> Value parallel_deterministic_reduce(
>     const Range& range, Value identity,
>     const Body& body, const Combine& combine,
>     const Partitioner& partitioner);
> ```
> **Semantics:** Bit-identical results across runs **if** `body` and `combine` are
> deterministic and the partitioner is **`simple_partitioner`** or
> **`static_partitioner`**. Do not use `auto_partitioner` here — it reintroduces
> schedule-dependent splits.

---

## 7.3.3 The cost: less adaptive load balancing

Determinism trades away the scheduler's freedom to rebalance fine-grained remainders
via stealing. On **uniform** workloads the penalty is often small. On **highly
skewed** workloads — one hot subrange costs 100× the rest — a fixed tree can leave
workers idle while the straggler finishes.

| | `parallel_reduce` | `parallel_deterministic_reduce` |
|---|-------------------|--------------------------------|
| Combine order | schedule-dependent | fixed by partitioner |
| Load balance | adaptive (stealing) | static/simple split plan |
| FP reproducibility | ✗ | ✓ (same partitioner + grain) |
| Best for | production throughput | tests, regulated numerics |

**Trade-offs ▸** Use **`parallel_reduce`** unless you **need** repeatability.
Use **`parallel_deterministic_reduce`** when bitwise-identical reruns matter more
than last-percentile throughput.

---

## 7.3.4 When reproducibility matters

```
   ✓ Unit / regression tests comparing golden values
   ✓ Financial or audit trails requiring identical reruns
   ✓ Scientific pipelines with strict validation protocols
   ✓ Debugging "works once, fails on re-run" heisenbugs

   ✗ HPC production where ±1 ULP drift is acceptable and speed wins
   ✗ Integer reductions with no overflow (often already deterministic enough)
```

Alternative for floats: accumulate in **`long double`** or **Kahan summation** in
serial post-pass, or use **`parallel_reduce`** and compare with epsilon in tests.
Deterministic reduce is the TBB-native fix when the parallel tree itself must match.

> **Pitfall ▸** Determinism requires a **deterministic body**. If your body reads
> from a race-prone global or calls `rand()` without fixed seeds, results still
> vary. Fix data races first (Part 7.5).

---

## 7.3.5 Example: comparing the two reducers

```cpp
// g++ -std=c++17 -O2 deterministic_reduce.cpp -ltbb
#include <oneapi/tbb/parallel_reduce.h>
#include <oneapi/tbb/blocked_range.h>
#include <oneapi/tbb/partitioner.h>
#include <cmath>
#include <cstdio>
#include <functional>
#include <vector>

int main() {
    const int N = 2'000'000;
    std::vector<double> data(N);
    for (int i = 0; i < N; ++i)
        data[i] = 1.0 / (i + 1);  // harmonic-ish terms → FP sensitivity

    const tbb::blocked_range<int> range(0, N, 10'000);

    auto body = [&](const tbb::blocked_range<int>& r, double sum) {
        for (int i = r.begin(); i != r.end(); ++i)
            sum += data[i];
        return sum;
    };
    auto combine = std::plus<double>{};

    double r1 = tbb::parallel_reduce(range, 0.0, body, combine);
    double r2 = tbb::parallel_reduce(range, 0.0, body, combine);

    double d1 = tbb::parallel_deterministic_reduce(
        range, 0.0, body, combine,
        tbb::simple_partitioner{});
    double d2 = tbb::parallel_deterministic_reduce(
        range, 0.0, body, combine,
        tbb::simple_partitioner{});

    double serial = 0.0;
    for (double x : data) serial += x;

    std::printf("serial reference:     %.17g\n", serial);
    std::printf("parallel_reduce #1:   %.17g\n", r1);
    std::printf("parallel_reduce #2:   %.17g\n", r2);
    std::printf("same as run #1?       %s\n",
        r1 == r2 ? "yes" : "NO — order differed");
    std::printf("deterministic #1:     %.17g\n", d1);
    std::printf("deterministic #2:     %.17g\n", d2);
    std::printf("bit-identical?        %s\n",
        d1 == d2 ? "yes" : "NO");

    return 0;
}
```

Run twice under load: `parallel_reduce` lines may differ from each other in the
last bits; deterministic lines match exactly. Both may differ slightly from serial
— that is expected FP behavior; deterministic reduce matches **across parallel
runs**, not necessarily against serial order.

> **Tuning ▸** Pass an explicit **`blocked_range` grainsize** (Part 0.4) with
> `simple_partitioner` so the tree depends only on `N` and grain. Larger grains
> reduce task overhead but coarsen the fixed tree — same reproducibility trade as
> in Part 0.5.

---

## Summary

- **`parallel_reduce`** combine order follows **adaptive scheduling** →
  floating-point results can differ run to run.
- **`parallel_deterministic_reduce`** uses a **fixed split tree** (with
  `simple_partitioner` / `static_partitioner`) → **bit-identical** reruns.
- Cost: **less adaptive load balancing** on skewed workloads — prefer standard
  `parallel_reduce` when reproducibility is not required.
- Use deterministic reduce for **tests, finance, science validation**; keep bodies
  and combines free of races and nondeterministic side effects.
- See Part 1.2 for the general reduce pattern; Part 0.5 for partitioner mechanics.

Next: [7.4 — Performance tuning & Amdahl's law](04-performance-tuning.md)
