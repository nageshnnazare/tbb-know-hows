# 6.2 — Function & Buffer Nodes

Part 6.1 introduced the graph skeleton: nodes, edges, reactive execution. This
chapter covers the workhorse **functional nodes** (`function_node`, `continue_node`,
`multifunction_node`) and the **buffering nodes** that decouple producers from
consumers — each with distinct ordering and capacity guarantees.

---

## 6.2.1 function_node — compute with controlled concurrency

A `function_node<In, Out>` applies a body to each incoming message and forwards
the result to successors:

```
   message In ──▶ [ body(In) → Out ] ──▶ successor(s)
                       │
              concurrency limit
              (serial / 1 / N / unlimited)
```

> **The API ▸**
>
> ```cpp
> #include <oneapi/tbb/flow_graph.h>
> using namespace tbb::flow;
>
> function_node<int, int> fn(g, unlimited, [](int x){ return x + 1; });
> function_node<int, void> sink(g, serial,       [](int x){ print(x); });
> // concurrency: serial (=1), 1, N, unlimited
> fn.try_put(10);
> ```
>
> `function_node<In, void>` consumes without output. Body must not throw if
> exceptions would leave the graph in an undefined state — catch inside.

| Concurrency | Semantics |
|-------------|-----------|
| `serial` / `1` | at most one body invocation at a time — preserves order |
| `N` | at most N concurrent invocations |
| `unlimited` | each message may spawn an independent task immediately |

**Trade-offs ▸** `unlimited` maximizes throughput when bodies are independent and
cheap. Use `serial` when the body touches non-thread-safe state (file append,
ordered logging). Use explicit `N` to cap resource use without full serialization.

---

## 6.2.2 continue_node — dependency without data

`continue_node` fires on a **`continue_msg`** — a signal with no payload. Use it
for pure dependency edges: "run B after A completes."

```cpp
continue_node<continue_msg> step_a(g, [](const continue_msg&) {
    std::printf("A done\n");
});
continue_node<continue_msg> step_b(g, [](const continue_msg&) {
    std::printf("B after A\n");
});
make_edge(step_a, step_b);
step_a.try_put(continue_msg());   // kick the chain
g.wait_for_all();
```

Pair with `join_node` (Part 6.3) when B must wait for **multiple** predecessors.
`continue_node` also accepts an optional **`node_priority_t`** (Part 6.4).

---

## 6.2.3 Buffering nodes — what each guarantees

Buffering nodes sit between fast producers and slow consumers. They differ in
**ordering** and **discarding** behavior:

```
   producer ──▶ [ buffer ] ──▶ consumer
                  │
         ordering policy + capacity semantics
```

| Node | Ordering guarantee | Notes |
|------|-------------------|-------|
| `buffer_node<T>` | none (arbitrary removal) | general hold; `try_get` any buffered item |
| `queue_node<T>` | FIFO | classic producer-consumer queue |
| `priority_queue_node<T>` | highest priority first | `T` must define `operator<` |
| `sequencer_node<T>` | ascending `T::operator()` key | reorder out-of-order stream |
| `overwrite_node<T>` | keeps **latest** only | drops older messages |

> **The API ▸**
>
> ```cpp
> buffer_node<int>        buf(g);
> queue_node<int>         q(g);
> priority_queue_node<Task> pq(g);   // Task::operator< defines priority
> sequencer_node<ordered_item> seq(g, [](const ordered_item& x){ return x.seq; });
> overwrite_node<int>     ow(g);
> ```

**Under the hood ▸** Buffering nodes are **graph_node + receiver + sender**. They
accept messages via `try_put` from predecessors (or manual injection) and hold
them until a successor `try_get`s or an edge push succeeds. Unbounded buffers
can grow without limit — combine with `limiter_node` (Part 6.4) upstream.

> **Pitfall ▸** A `buffer_node` between a fast producer and `serial` consumer
> hides back-pressure — memory grows silently. If that is not intentional, add a
> `limiter_node` or use `overwrite_node` when only the latest value matters.

---

## 6.2.4 multifunction_node (brief)

When one input must route to **multiple output ports** conditionally:

```cpp
using split_t = multifunction_node<int, std::tuple<int, int>>;
split_t router(g, unlimited,
    [](int x, split_t::output_ports_type& ports) {
        if (x % 2 == 0)
            std::get<0>(ports).try_put(x);
        else
            std::get<1>(ports).try_put(x);
    });
make_edge(output_port<0>(router), even_handler);
make_edge(output_port<1>(router), odd_handler);
```

Unlike `function_node`, the body pushes to named output ports explicitly — a
switchboard inside the graph. Heavier than simple edges; use when topology is
data-dependent.

---

## 6.2.5 Example: wiring function_nodes in a small pipeline

Three compute stages with a queue buffer before a serial writer:

```cpp
// build: g++ -std=c++17 -O2 func_nodes.cpp -ltbb -o func_nodes
#include <oneapi/tbb/flow_graph.h>
#include <cstdio>

int main() {
    using namespace tbb::flow;
    graph g;

    function_node<int, int> gen(g, unlimited,
        [](int x) { return x; });

    function_node<int, int> compute(g, unlimited,
        [](int x) { return x * x; });

    queue_node<int> buffer(g);   // FIFO between compute and output

    function_node<int, void> output(g, serial,
        [](int x) { std::printf("  out: %d\n", x); });

    make_edge(gen, compute);
    make_edge(compute, buffer);
    make_edge(buffer, output);

    for (int i = 1; i <= 5; ++i)
        gen.try_put(i);

    g.wait_for_all();
    return 0;
}
```

```
   gen ──▶ compute ──▶ queue_node (FIFO) ──▶ output (serial, ordered print)
```

The queue decouples parallel `compute` from serial `output`; FIFO ensures print
order matches completion order *as seen by the queue*, not necessarily submission
order from `gen`.

---

## Summary

- **`function_node<In,Out>`** transforms messages; concurrency is
  `serial` / `N` / `unlimited`.
- **`continue_node`** expresses **dependencies** via `continue_msg` without data.
- Buffering nodes differ by **ordering**: arbitrary (`buffer_node`), FIFO
  (`queue_node`), priority (`priority_queue_node`), keyed (`sequencer_node`),
  latest-only (`overwrite_node`).
- **`multifunction_node`** routes one input to multiple output ports in the body.
- Match concurrency to thread safety; unbounded buffers need explicit back-pressure
  (Part 6.4).

Next: [6.3 — Join & split nodes](03-join-and-split-nodes.md)
