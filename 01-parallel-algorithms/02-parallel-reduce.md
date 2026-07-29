# 1.2 — parallel_reduce

When iterations are not independent because they **combine** into one result — a sum, a
minimum, a custom aggregate — use `parallel_reduce`. The pattern is map-reduce on index
ranges: each task folds its subrange into a **partial value**, then partials are **joined**
up the split tree (same splitting machinery as `parallel_for`, Part 1.1).

```
   range split → partial₀  partial₁  partial₂  partial₃
                      └──────── join tree ────────┘
                                  result
```

---

## 1.2.1 Functional form: identity, body-fold, join

The most common API takes four callables:

```cpp
Result r = tbb::parallel_reduce(
    range,
    identity,           // starting value for an empty range
    body,               // (subrange, local_init) → local_result
    join);              // (partial_a, partial_b) → combined
```

The body receives a **local copy** of the running accumulator (`init`) for its subrange.
The join combines two partial results after subtrees finish.

> **The API ▸**
> ```cpp
> // <oneapi/tbb/parallel_reduce.h>
> template<typename Range, typename Value, typename Body, typename Reduction>
> Value parallel_reduce(const Range& range, Value identity, const Body& body,
>                       const Reduction& reduction);
> template<typename Range, typename Body>
> void parallel_reduce(const Range& range, Body& body);  // splitting body — see §1.2.2
> ```
> **Semantics:** Returns the reduced value (functional form). The splitting-body overload
> mutates `body` in place to hold the final result.

![parallel_reduce: partial folds per subrange, join up the split tree](figures/parallel-reduce.svg)

---

## 1.2.2 Imperative form: splitting body + join

For stateful bodies (multiple member fields, reuse across calls), implement a **class**
with a **split constructor**:

```cpp
struct DotProduct {
    const std::vector<double> &a, &b;
    double sum;

    DotProduct(const std::vector<double>& a_, const std::vector<double>& b_)
        : a(a_), b(b_), sum(0) {}

    DotProduct(DotProduct& x, tbb::split)   // called when range splits
        : a(x.a), b(x.b), sum(0) {}

    void operator()(const tbb::blocked_range<size_t>& r) {
        for (size_t i = r.begin(); i != r.end(); ++i)
            sum += a[i] * b[i];
    }

    void join(const DotProduct& y) { sum += y.sum; }
};

DotProduct body(a, b);
tbb::parallel_reduce(tbb::blocked_range<size_t>(0, a.size()), body);
// body.sum holds the result
```

```
   parent body (sum=0)
        split
   left body (sum=0)     right body (sum=0)
        │                      │
   operator()(subrange)   operator()(subrange)
        │                      │
        └──── join() ──────────┘
              parent.sum = left.sum + right.sum
```

> **Under the hood ▸** The split constructor creates a **sibling** body with a fresh
> accumulator. `join` merges a completed sibling into the caller. This mirrors the task
> tree: each leaf runs `operator()`, internal nodes run `join`.

---

## 1.2.3 Associativity requirement

`join` must be **associative**: grouping must not change the mathematical result for exact
types.

```
   (a ⊕ b) ⊕ c  ==  a ⊕ (b ⊕ c)     required
```

Commutativity is **not** strictly required (tree shape fixes order among siblings), but
non-commutative joins produce results that depend on split order — usually undesirable.

| Operation | Associative? | Notes |
|-----------|--------------|-------|
| Integer sum | ✓ | Exact |
| `min` / `max` | ✓ | Exact |
| Floating-point sum | ≈ | Rounding breaks strict associativity |
| Matrix multiply | ✓ | Dimension must match in join |
| "First index where pred" | ✗ | Use a different pattern |

> **Pitfall ▸** Using `parallel_reduce` with a non-associative "join" (e.g. string concat
> where order matters, or "keep smallest index with value X" without tie logic) yields
> **nondeterministic wrong answers**, not just nondeterministic ordering.

---

## 1.2.4 Floating-point: not bit-identical across runs

Parallel fold order differs run-to-run as stealers take different branches. For `float` /
`double`, `(a + b) + c ≠ a + (b + c)` in general, so:

```
   serial sum:     1.0 + 1e-16 + 1e-16 + ...  → 1.0
   parallel sum:   (1.0 + 1e-16) + ...       → may differ in low bits
```

This is **not a TBB bug** — it is IEEE-754 non-associativity amplified by parallel tree
reduction. For bitwise-reproducible floating-point sums, use
`parallel_deterministic_reduce` (Part 7.3) or fixed-order serial accumulation when
required.

**Trade-offs ▸** `parallel_reduce` is faster and more scalable; deterministic variants
trade performance for repeatability.

---

## 1.2.5 Example: sum of integers

```cpp
// g++ -std=c++17 -O2 reduce_sum.cpp -ltbb
#include <oneapi/tbb/parallel_reduce.h>
#include <oneapi/tbb/blocked_range.h>
#include <cstdio>
#include <vector>

int main() {
    const size_t n = 50'000'000;
    std::vector<int> data(n);
    for (size_t i = 0; i < n; ++i)
        data[i] = static_cast<int>(i & 0xFF);

    long long total = tbb::parallel_reduce(
        tbb::blocked_range<size_t>(0, n),
        0LL,
        [&](const tbb::blocked_range<size_t>& r, long long init) {
            for (size_t i = r.begin(); i != r.end(); ++i)
                init += data[i];
            return init;
        },
        [](long long a, long long b) { return a + b; });

    std::printf("sum = %lld\n", total);
    return 0;
}
```

---

## 1.2.6 Example: maximum value

`min`/`max` reductions need an identity that is the **neutral element** for the join:

```cpp
// g++ -std=c++17 -O2 reduce_max.cpp -ltbb
#include <oneapi/tbb/parallel_reduce.h>
#include <oneapi/tbb/blocked_range.h>
#include <algorithm>
#include <cstdio>
#include <limits>
#include <vector>

int main() {
    const size_t n = 20'000'000;
    std::vector<double> data(n);
    for (size_t i = 0; i < n; ++i)
        data[i] = std::sin(i * 0.0003) * 1000.0;

    double mx = tbb::parallel_reduce(
        tbb::blocked_range<size_t>(0, n),
        -std::numeric_limits<double>::infinity(),
        [&](const tbb::blocked_range<size_t>& r, double cur) {
            for (size_t i = r.begin(); i != r.end(); ++i)
                cur = std::max(cur, data[i]);
            return cur;
        },
        [](double a, double b) { return std::max(a, b); });

    std::printf("max = %.6f\n", mx);
    return 0;
}
```

**Tuning ▸** Same grainsize guidance as `parallel_for` (Part 0.4): each `operator()` /
body invocation should do enough work to amortize split/join overhead.

---

## Summary

- `parallel_reduce` folds independent subranges with a **join** that must be
  **associative**.
- **Functional form:** `identity` + `(range, init) → partial` + `join`.
- **Imperative form:** class with **`split` constructor**, `operator()(range)`,
  `join(sibling)`.
- **Floating-point** results vary in low bits across runs; see Part 7.3 for
  deterministic reduction.
- Sum and max are the canonical patterns; custom structs work when `join` is associative.

Next: [1.3 — parallel_scan](03-parallel-scan.md)
