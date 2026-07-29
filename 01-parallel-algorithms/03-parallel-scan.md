# 1.3 — parallel_scan

`parallel_scan` computes a **prefix operation** over a range: each output element
depends on all previous inputs (inclusive or exclusive, depending on your body). Unlike
`parallel_for`, iterations have **intra-loop dependency** — but the algorithm still
extracts parallelism via a two-pass **upsweep / downsweep** strategy at the cost of
~**2× the serial work**.

```
   serial:  out[i] = f(in[0..i])        one pass, no parallelism

   parallel_scan:
        pass 1 (pre-scan):  partial sums per chunk  ─┐
        pass 2 (final-scan): write outputs + propagate ┘  ~2× reads/writes
```

Use it for prefix sums, stream compaction, parallel allocation from flags, and any
"running aggregate" where `parallel_for` is wrong but a associative operator exists.

---

## 1.3.1 Prefix-sum semantics

For an inclusive prefix sum with `+`:

```
   input:  [1, 2, 3, 4, 5]
   output: [1, 3, 6, 10, 15]     output[i] = sum(input[0..i])
```

Exclusive scan would shift the write: `output[i] = sum(input[0..i-1])`, `output[0] = 0`.

The operator must be **associative** (same requirement as `parallel_reduce`, Part 1.2).
Scan additionally threads a **running prefix** through chunks in a defined order.

![parallel_scan two-pass: pre-scan partials, then final-scan with offsets](figures/parallel-scan.svg)

---

## 1.3.2 Two-pass algorithm: pre-scan and final-scan

Mechanically, oneTBB runs two logical phases over the split tree:

```
   Phase 1 — pre-scan (upsweep):
      Each leaf computes aggregate of its subrange (no output write, or temp only).
      Internal nodes combine child aggregates bottom-up.

   Phase 2 — final-scan (downsweep):
      Propagate prefix from the root down.
      Each leaf writes output[i] when it knows the prefix before index i.
```

```
        chunk₀   chunk₁   chunk₂   chunk₃
          │        │        │        │
   pre-scan:  3       7        5        9     (local aggregates)
          └────┬─────┴────┬─────┘
               10        14                  (combined)
                    24                     (total)

   final-scan: prefix 0 → 3 → 10 → 15 flows down; each chunk writes its outputs
```

> **Under the hood ▸** The body may be invoked **multiple times** on overlapping index
> ranges across passes. That is why the API distinguishes pre-scan vs final-scan — you
> must **not** write the output array unless you are in the final pass.

---

## 1.3.3 Body with `is_final_scan` tag (class form)

The class-based body uses a tagged `operator()`:

```cpp
template<typename Tag>
void operator()(const tbb::blocked_range<size_t>& r, Tag) {
    int tmp = sum;
    for (size_t i = r.begin(); i != r.end(); ++i) {
        tmp += input[i];
        if (Tag::is_final_scan())
            output[i] = tmp;    // write ONLY on final pass
    }
    sum = tmp;
}
```

Required members for the class form:

| Member | Role |
|--------|------|
| `Body(Body& other, split)` | Fresh accumulator when range splits |
| `operator()(range, Tag)` | Fold; write output iff `Tag::is_final_scan()` |
| `reverse_join(Body& left)` | Merge prefix from left sibling: `sum = left.sum + sum` |
| `assign(Body& other)` | Copy aggregate from completed subtree (used internally) |

> **The API ▸**
> ```cpp
> // <oneapi/tbb/parallel_scan.h>
> template<typename Range, typename Body>
> void parallel_scan(const Range& range, Body& body);
> template<typename Range, typename Value, typename Scan, typename Merge>
> void parallel_scan(const Range& range, Value identity, const Scan& scan,
>                    const Merge& merge);
> ```
> Functional overload: scan lambda receives `(range, sum, bool is_final)`; return updated
> running sum. Merge combines prefixes like `parallel_reduce`'s join.

---

## 1.3.4 `reverse_join` direction

After pre-scan, siblings must combine so the **right** subtree knows the **left**
subtree's total:

```
   left.sum = aggregate of indices [0..k)
   right.sum = aggregate of indices [k..m)

   reverse_join(left):  right.sum = left.sum + right.sum
                        (prefix for right chunk includes all left input)
```

Getting join direction wrong produces shifted or garbage prefixes — the common bug when
porting a hand-rolled scan.

---

## 1.3.5 ~2× work vs serial

A serial prefix sum touches each element once. `parallel_scan` reads/writes each element
in **two passes** over the tree structure, so the theoretical work multiplier is ~2×
before accounting for synchronization.

**Trade-offs ▸**

| | Serial `std::partial_sum` | `parallel_scan` |
|---|---------------------------|-----------------|
| Work | O(n) | ~O(n) with higher constant |
| Span | O(n) | O(log n) with p processors |
| Wins when | Small n, low latency | Large n, many cores, expensive body |

For `n = 10⁴` on a laptop, serial often wins. For `n = 10⁸` on 32 cores, parallel scan
can dominate.

> **Pitfall ▸** Writing `output[i]` on the pre-scan pass races with final-scan writes.
> Always guard with `Tag::is_final_scan()` or `if (is_final)`.

---

## 1.3.6 Inclusive-scan example

```cpp
// g++ -std=c++17 -O2 parallel_scan_demo.cpp -ltbb
#include <oneapi/tbb/parallel_scan.h>
#include <oneapi/tbb/blocked_range.h>
#include <cstdio>
#include <numeric>
#include <vector>

class InclusivePrefixSum {
    const std::vector<int>& in;
    std::vector<int>& out;
    int sum;

public:
    InclusivePrefixSum(const std::vector<int>& in_, std::vector<int>& out_)
        : in(in_), out(out_), sum(0) {}

    InclusivePrefixSum(InclusivePrefixSum& b, tbb::split)
        : in(b.in), out(b.out), sum(0) {}

    template<typename Tag>
    void operator()(const tbb::blocked_range<size_t>& r, Tag) {
        int tmp = sum;
        for (size_t i = r.begin(); i != r.end(); ++i) {
            tmp += in[i];
            if (Tag::is_final_scan())
                out[i] = tmp;
        }
        sum = tmp;
    }

    void reverse_join(InclusivePrefixSum& left) { sum = left.sum + sum; }
    void assign(InclusivePrefixSum& other) { sum = other.sum; }
};

int main() {
    std::vector<int> input = {1, 2, 3, 4, 5, 6, 7, 8};
    std::vector<int> output(input.size());

    InclusivePrefixSum body(input, output);
    tbb::parallel_scan(tbb::blocked_range<size_t>(0, input.size()), body);

    std::vector<int> expected(input.size());
    std::partial_sum(input.begin(), input.end(), expected.begin());

    std::printf("output: ");
    for (int x : output) std::printf("%d ", x);
    std::printf("\nexpected: ");
    for (int x : expected) std::printf("%d ", x);
    std::printf("\n");
    return 0;
}
```

Lambda equivalent for large `n`:

```cpp
tbb::parallel_scan(
    tbb::blocked_range<size_t>(0, n),
    0,
    [&](const tbb::blocked_range<size_t>& r, int prefix, bool is_final) {
        for (size_t i = r.begin(); i != r.end(); ++i) {
            prefix += input[i];
            if (is_final) output[i] = prefix;
        }
        return prefix;
    },
    [](int a, int b) { return a + b; });
```

---

## Summary

- `parallel_scan` implements **prefix operations** with an associative operator.
- Two passes — **pre-scan** (aggregates) and **final-scan** (writes) — enable parallelism
  at ~**2× serial work**.
- Guard output writes with **`Tag::is_final_scan()`** or `bool is_final`.
- **`reverse_join`** adds the left sibling's aggregate into the right prefix.
- Prefer serial scan for small `n`; use parallel scan for large arrays on many cores.

Next: [1.4 — parallel_sort](04-parallel-sort.md)
