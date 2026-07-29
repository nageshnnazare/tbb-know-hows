# 6.3 — Join & Split Nodes

Part 6.2 covered nodes that transform or buffer a **single** stream. Real graphs
**fan out** one message to many successors and **fan in** many streams into one
synchronized tuple. This chapter covers `join_node`, `split_node`, and
`broadcast_node` — the primitives behind fork-join dataflow patterns.

---

## 6.3.1 Fan-out and fan-in

```
   FAN-OUT                         FAN-IN
   ───────                         ──────

        ┌──▶ branch A                      stream A ──┐
   src ─┼──▶ branch B                              ├──▶ join ──▶ tuple
        └──▶ branch C                      stream B ──┘
```

- **Fan-out**: one producer, many consumers (`broadcast_node`, or multiple
  `make_edge` from one source).
- **Fan-in**: many producers, one consumer that needs **all** inputs before
  proceeding (`join_node`).

![join_node waits for one message on each input port, emits a tuple to successors](figures/join-node.svg)

---

## 6.3.2 join_node and its policies

> **The API ▸**
>
> ```cpp
> join_node<std::tuple<int, std::string>> j(g);                    // default policy
> join_node<std::tuple<int, int>, queueing> jq(g);
> join_node<std::tuple<int, int>, reserving> jr(g);
> // key_matching: pair by tag extracted from each message type
> ```
>
> Connect inputs: `make_edge(src0, input_port<0>(j));`
> Output: `std::tuple<...>` broadcast to successors when join completes.

Three buffering policies — **all ports on one join must use the same policy**:

| Policy | Mechanism | Pairing rule |
|--------|-----------|--------------|
| **`queueing`** (default) | buffers one message **per port** | emits when **each** port has ≥1 message; FIFO pairing across ports |
| **`reserving`** | pull/reserve protocol | successor reserves; join pulls from all ports atomically when all ready |
| **`key_matching<K>`** | tag-indexed slots | pairs messages whose **keys match** — streams can arrive in any order |

**queueing** — simplest mental model: think of one slot per input pipe; when all
pipes have a message, pop one from each and form the tuple:

```
   port0: [3] [7]        port1: [x] [y]
          └───┬─── when both heads ready → (3, x), then (7, y)
              ▼
           tuple out
```

**reserving** — better when you want the **consumer to pull** only when it can
take a complete tuple, reducing internal buffering under back-pressure.

**key_matching** — when streams are **not aligned by arrival order** but carry
matching IDs (e.g., `(id=5, data)` with `(id=5, metadata)`):

```cpp
struct tagged_int { int key, value; };
struct tagged_str { int key; std::string text; };

join_node<std::tuple<tagged_int, tagged_str>, key_matching<int>> j(g,
    [](const tagged_int& m) { return m.key; },
    [](const tagged_str& m) { return m.key; });
```

> **Pitfall ▸** With **queueing**, a fast port fills its one-slot buffer while a
> slow port stalls — the join blocks until the slow port delivers. A burst on
> port 0 without port 1 does not pair; design stream rates or use
> **key_matching** when order across ports is uncorrelated.

---

## 6.3.3 split_node — tuple to separate streams

The inverse of join: one `std::tuple` message becomes one message per output port:

```cpp
split_node<std::tuple<int, double>> splitter(g);
function_node<int, void>    int_sink(g, unlimited, [](int v){ ... });
function_node<double, void> dbl_sink(g, unlimited, [](double v){ ... });

make_edge(joiner, splitter);
make_edge(output_port<0>(splitter), int_sink);
make_edge(output_port<1>(splitter), dbl_sink);
```

Typical pattern: **join** heterogeneous branches → process combined → **split**
again for the next divergent stage.

---

## 6.3.4 broadcast_node — fan-out one message to all

`broadcast_node<T>` forwards **every** message to **all** connected successors:

```cpp
broadcast_node<int> bc(g);
function_node<int, void> c1(g, unlimited, [](int x){ ... });
function_node<int, void> c2(g, unlimited, [](int x){ ... });
make_edge(bc, c1);
make_edge(bc, c2);
bc.try_put(42);   // both c1 and c2 receive 42
```

Unlike making two edges from a `function_node` output (which sends one copy
through the graph's push semantics), `broadcast_node` is explicit fan-out — useful
with `continue_msg` to kick parallel branches (Part 6.4 priorities example).

---

## 6.3.5 Example: joining two streams

Two generators produce derived values; join waits for both before summing:

```cpp
// build: g++ -std=c++17 -O2 join_two.cpp -ltbb -o join_two
#include <oneapi/tbb/flow_graph.h>
#include <cstdio>
#include <string>
#include <tuple>

int main() {
    using namespace tbb::flow;
    using joined = std::tuple<int, std::string>;

    graph g;

    function_node<int, int> nums(g, unlimited,
        [](int k) { return k * 10; });

    function_node<int, std::string> labels(g, unlimited,
        [](int k) { return "item_" + std::to_string(k); });

    join_node<joined> joiner(g);

    function_node<joined, void> sink(g, unlimited,
        [](joined t) {
            std::printf("  joined (%d, %s)\n", std::get<0>(t), std::get<1>(t).c_str());
        });

    make_edge(nums,   input_port<0>(joiner));
    make_edge(labels, input_port<1>(joiner));
    make_edge(joiner, sink);

    for (int k = 1; k <= 3; ++k) {
        nums.try_put(k);
        labels.try_put(k);   // both must arrive before join fires
    }

    g.wait_for_all();
    return 0;
}
```

Output (order may vary by scheduling):

```
   joined (10, item_1)
   joined (20, item_2)
   joined (30, item_3)
```

With **queueing**, the k-th message on each port pairs by FIFO position — so
`nums.try_put(1); nums.try_put(2); labels.try_put(1);` pairs `(10, item_1)` only
after `labels` catches up; mis-ordered injection causes **wrong pairings**, not
deadlock.

**Tuning ▸** When pairing is by ID not arrival order, switch to
**`key_matching`**. When join is a synchronization barrier for `continue_msg`
only, `join_node<std::tuple<continue_msg, continue_msg>>` works with
`continue_node` chains (Part 6.1 dependency example).

---

## Summary

- **`join_node<tuple<...>>`** waits for inputs on all ports, emits one tuple;
  policy controls pairing: **queueing**, **reserving**, or **key_matching**.
- **`split_node<tuple<...>>`** fans a tuple out to separate output ports.
- **`broadcast_node<T>`** sends each message to **every** successor — explicit
  fan-out.
- Queueing join pairs by **FIFO position per port** — injection order matters.
- Use **key_matching** when streams align by **tag**, not by arrival sequence.

Next: [6.4 — Priorities & limiters](04-priorities-and-limiters.md)
