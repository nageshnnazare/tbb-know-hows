# 1.6 — parallel_pipeline

`parallel_pipeline` models **streaming assembly lines**: a sequence of **filters** process
**tokens** (items) flowing stage to stage. Unlike `parallel_for` (all iterations known
upfront), a pipeline overlaps **produce → transform → consume** so memory bandwidth and
CPU can stay busy on different items simultaneously.

```
   token 0:  [read]──▶[transform]──▶[write]
   token 1:       [read]──▶[transform]──▶[write]
   token 2:             [read]──▶[transform]──▶[write]
              serial?     parallel?    serial in-order?
```

---

## 1.6.1 Filters and filter modes

Each stage is a `make_filter<Input, Output>(mode, body)` chained with **`operator&`**:

> **The API ▸**
> ```cpp
> // <oneapi/tbb/parallel_pipeline.h>
> enum class filter_mode {
>     parallel,            // multiple items at once, any order
>     serial_in_order,     // one at a time, preserves stream order
>     serial_out_of_order  // one at a time, order not preserved
> };
> template<typename T, typename U, typename Body>
> filter<T,U> make_filter(filter_mode mode, Body body);
> void parallel_pipeline(size_t max_number_of_live_tokens,
>                        filter<void,void> chain);
> ```

| Mode | Concurrency | Order |
|------|-------------|-------|
| `parallel` | Many workers on distinct tokens | Unordered among tokens |
| `serial_in_order` | One at a time | **Global order** from first serial_in_order filter |
| `serial_out_of_order` | One at a time | Arbitrary order |

![parallel_pipeline: tokens flow through serial and parallel filter stages](figures/parallel-pipeline.svg)

The **first** filter's input type must be `void` (source); the **last** filter's output
type must be `void` (sink). Middle filters pass typed values token to token.

---

## 1.6.2 Tokens and `max_number_of_live_tokens`

The first parameter caps **in-flight items** — back-pressure for the whole pipeline:

```
   max_tokens = 3

   [read] can produce at most 3 items not yet consumed by final stage
         → limits memory if each token is a large buffer
         → bounds queue depth between slow/fast stages
```

Rule of thumb: set to a small multiple of the number of parallel stage workers (often 4–16).
Too few → starvation; too many → memory bloat.

> **Under the hood ▸** Tokens are pipeline scheduling units, not OS threads. A parallel
> filter may process different tokens on different workers simultaneously; serial_in_order
> filters delay invoking the body until the token's turn in the global sequence.

---

## 1.6.3 Order preservation

All `serial_in_order` filters share one **implicit sequence number** established by the
**first** such filter in the chain. If transform is parallel but write is
`serial_in_order`, the runtime **buffers** completed items until predecessors are written
— output order matches input order even when transforms finish out of order.

```
   read order:     A  B  C  D
   transform finish: C  A  D  B   (parallel stage)
   write order:    A  B  C  D     (serial_in_order delays C, D until A, B done)
```

Use `serial_out_of_order` for a serial bottleneck that does **not** need ordering (e.g.
aggregating into a thread-safe counter).

---

## 1.6.4 Combining filters with `operator&`

```cpp
auto chain =
    make_filter<void, Item>(filter_mode::serial_in_order, read_body) &
    make_filter<Item, Item>(filter_mode::parallel, transform_body) &
    make_filter<Item, void>(filter_mode::serial_in_order, write_body);

tbb::parallel_pipeline(max_tokens, chain);
```

Filters are composable at compile time; the combined type is `filter<void,void>`.

**Pitfall ▸** Mismatching input/output types between adjacent filters is a compile error.
Forgetting `void` on the ends breaks the chain template matching.

---

## 1.6.5 Three-stage read → transform → write

```cpp
// g++ -std=c++17 -O2 pipeline_demo.cpp -ltbb
#include <oneapi/tbb/parallel_pipeline.h>
#include <cstdio>
#include <fstream>
#include <string>
#include <vector>

struct Record {
    int id;
    std::string text;
};

int main() {
    // Write a tiny input file
    {
        std::ofstream out("input.txt");
        for (int i = 0; i < 100; ++i)
            out << "line-" << i << "\n";
    }

    std::ifstream in("input.txt");
    int next_id = 0;
    std::vector<std::string> output;
    output.reserve(100);

    const size_t max_tokens = 8;

    tbb::parallel_pipeline(
        max_tokens,
        tbb::make_filter<void, Record>(
            tbb::filter_mode::serial_in_order,
            [&](tbb::flow_control& fc) -> Record {
                std::string line;
                if (!std::getline(in, line)) {
                    fc.stop();
                    return {};
                }
                return Record{next_id++, std::move(line)};
            }) &
        tbb::make_filter<Record, Record>(
            tbb::filter_mode::parallel,
            [](Record rec) -> Record {
                // CPU-heavy transform (uppercase)
                for (char& c : rec.text)
                    if (c >= 'a' && c <= 'z') c -= 32;
                return rec;
            }) &
        tbb::make_filter<Record, void>(
            tbb::filter_mode::serial_in_order,
            [&](Record rec) {
                output.push_back(rec.text);
            }));

    std::printf("processed %zu records\n", output.size());
    std::printf("first=%s  last=%s\n", output.front().c_str(), output.back().c_str());
    return 0;
}
```

Source filter stops the pipeline with **`fc.stop()`** when input is exhausted; return value
on stop is ignored.

---

## 1.6.6 When to use pipeline vs other tools

```
   ✓  Streaming I/O + CPU stages with different throughput
   ✓  Bounded memory on unbounded streams (cap tokens)
   ✓  Overlap read/compute/write on large files

   ✗  All stages same cost, batch already in memory → parallel_for
   ✗  Complex DAG dependencies → Flow Graph (Part 6)
   ✗  Two-way fork only → parallel_invoke (Part 1.5)
```

**Trade-offs ▸** Pipeline adds latency for the first output (must fill the pipe) but
improves **throughput** on sustained streams. Tuning `max_tokens` is the main knob.

---

## Summary

- `parallel_pipeline` chains **filters** with `make_filter` and **`operator&`**.
- Modes: **`parallel`**, **`serial_in_order`**, **`serial_out_of_order`**.
- **`max_number_of_live_tokens`** limits in-flight items (back-pressure / memory).
- **`serial_in_order`** stages preserve global order; parallel stages may reorder internally.
- Ideal for **read → transform → write** streaming with overlapped stages.

Next: [2.1 — task_group](../02-task-programming/01-task-group.md)
