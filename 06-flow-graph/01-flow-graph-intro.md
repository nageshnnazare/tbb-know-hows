# 6.1 — Flow Graph Fundamentals

Parts 1 and 2 cover **regular** parallelism: loops, reductions, pipelines with a
fixed stage sequence. Many real problems are neither — they are **irregular DAGs**
of dependencies where stage B waits for A *and* C, where one source fans out to
five consumers, or where work arrives reactively. The **Flow Graph** expresses
that dependency structure directly: **nodes** (actors) connected by **edges**
(message channels), scheduled by the same work-stealing runtime as
`parallel_for`.

---

## 6.1.1 When parallel_for and pipeline aren't enough

```
   parallel_for     →  independent iterations of ONE loop
   parallel_pipeline →  linear stages, fixed order, token stream

   Flow Graph       →  arbitrary DAG: fork, join, broadcast, conditional paths
```

| Pattern | Fit |
|---------|-----|
| Square every element | `parallel_for` |
| Load → filter → save, in order | `parallel_pipeline` |
| Parse doc → (spell-check ∥ grammar-check) → merge → render | **Flow Graph** |
| Event-driven: message arrives → dispatch to handler graph | **Flow Graph** |

The graph model is **reactive**: a node runs when its **inputs arrive**, not when
you call it. That matches streaming servers, composable media pipelines, and
dependency graphs where fan-in/fan-out topology changes at compile time.

> **The key idea ▸** You draw the **dependency structure**; TBB schedules node
> bodies as tasks when messages flow. Same composability as algorithms — one
> scheduler, no thread explosion.

---

## 6.1.2 Graph, nodes, edges, messages

![A directed graph of nodes connected by edges; messages flow when predecessors produce](figures/flow-graph.svg)

```
   ┌─────────┐     edge      ┌─────────┐     edge      ┌─────────┐
   │ node A  │ ────────────▶ │ node B  │ ────────────▶ │ node C  │
   │ (actor) │   message     │ (actor) │   message     │ (actor) │
   └─────────┘               └─────────┘               └─────────┘

   graph g;                  // owns all nodes; drives task execution
   make_edge(a, b);         // a's output → b's input
   g.wait_for_all();        // block until quiescent
```

> **The API ▸**
>
> ```cpp
> #include <oneapi/tbb/flow_graph.h>
> using namespace tbb::flow;
>
> graph g;
> function_node<int, int> n(g, unlimited, [](int x){ return x * 2; });
> make_edge(source, n);
> n.try_put(42);           // inject a message (if source is manual)
> g.wait_for_all();
> ```
>
> Namespace: `tbb::flow`. Link `-ltbb`. Header pulls in nodes, policies, and
> `flow_control`.

**Under the hood ▸** Each node that receives a message enqueues a task on the
TBB scheduler. `unlimited` concurrency on a node means multiple messages may
execute concurrently on different workers; `serial` (or `1`) forces one-at-a-time
order. The graph tracks outstanding tasks and `wait_for_all()` joins them.

---

## 6.1.3 input_node — a proper source

Manual `try_put` on downstream nodes works for demos; production graphs use
**`input_node`** to generate a stream until stopped:

> **The API ▸**
>
> ```cpp
> input_node<int> src(g, [](flow_control& fc) -> int {
>     static int i = 0;
>     if (i < 10) return ++i;
>     fc.stop();              // stop generating
>     return 0;
> }, /*is_active=*/false);   // construct inactive, wire edges, then activate
> src.activate();
> ```
>
> Body signature: `Output(flow_control& fc)`. Call `fc.stop()` when done.
> Construct **inactive**, connect edges, then **`activate()`** — otherwise early
> messages may be dropped before successors exist.

---

## 6.1.4 Minimal working graph

A source generates integers; one branch squares, another cubes; a join adds the
results (join details in Part 6.3):

```cpp
// build: g++ -std=c++17 -O2 flow_minimal.cpp -ltbb -o flow_minimal
#include <oneapi/tbb/flow_graph.h>
#include <cstdio>
#include <tuple>

int main() {
    using namespace tbb::flow;

    graph g;
    int limit = 5;

    input_node<int> src(g, [&](flow_control& fc) -> int {
        static int n = 1;
        if (n <= limit) return n++;
        fc.stop();
        return 0;
    }, false);

    function_node<int, int> squarer(g, unlimited,
        [](int v) { return v * v; });

    function_node<int, int> cuber(g, unlimited,
        [](int v) { return v * v * v; });

    join_node<std::tuple<int, int>> joiner(g);

    function_node<std::tuple<int, int>, void> summer(g, unlimited,
        [](std::tuple<int, int> t) {
            std::printf("  %d² + %d³ = %d\n",
                std::get<0>(t), std::get<1>(t),
                std::get<0>(t) + std::get<1>(t));
        });

    make_edge(src, squarer);
    make_edge(src, cuber);
    make_edge(squarer, input_port<0>(joiner));
    make_edge(cuber,  input_port<1>(joiner));
    make_edge(joiner, summer);

    src.activate();
    g.wait_for_all();
    return 0;
}
```

Execution unfolds reactively:

```
   src emits 1 → squarer(1), cuber(1) → join (1,1) → summer prints
   src emits 2 → ...
   ...
   src stops   → graph drains → wait_for_all returns
```

> **Pitfall ▸** Wiring order matters for `input_node`: **edges first,
> `activate()` last**. Activating before successors are connected drops messages
> permanently.

---

## 6.1.5 Reactive dataflow vs task_group

```
   task_group          you push work explicitly: g.run([]{...});
   Flow Graph          work propagates when messages arrive along edges

   both → tasks on the same work-stealing scheduler (Part 0.3)
```

**Trade-offs ▸** Flow Graph adds node bookkeeping overhead — overkill for a
single parallel loop. It shines when topology is **nonlinear**, **persistent**
(the graph stays up and processes many message batches), or **heterogeneous**
(different node types: buffers, joins, limits — Parts 6.2–6.4). Debugging is
harder than `parallel_for`; log inside node bodies and cap in-flight messages
with `limiter_node` (Part 6.4).

**Tuning ▸** Start with `function_node` + `make_edge`. Add buffering nodes when
producer and consumer rates differ (Part 6.2). Add `join_node` when paths must
synchronize (Part 6.3). Profile with small message counts first — graph setup
bugs (dropped messages, wrong port index) show up before performance tuning.

---

## Summary

- Use the **Flow Graph** when dependencies form an **irregular DAG**, not a loop
  or a linear pipeline.
- A **`graph`** owns nodes; **`make_edge`** connects outputs to inputs; messages
  trigger node execution **reactively**.
- **`input_node`** generates messages until `flow_control::stop()`; wire edges,
  then **`activate()`**.
- **`g.wait_for_all()`** blocks until all graph tasks complete.
- Same TBB scheduler underneath — composable with `parallel_for` and containers.

Next: [6.2 — Function & buffer nodes](02-function-and-buffer-nodes.md)
