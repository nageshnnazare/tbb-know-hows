# 6.4 — Priorities & Limiters

Parts 6.1–6.3 built graphs that run quickly — this chapter keeps them from running
**too much at once**. Unbounded `unlimited` nodes plus fast sources can queue
millions of messages and blow RSS. **Node priorities** bias the scheduler toward
critical paths; **`limiter_node`** enforces hard caps on in-flight work with an
explicit decrement loop for back-pressure.

---

## 6.4.1 Why unbounded graphs hurt

```
   fast input_node ──▶ unlimited compute ──▶ slow output
                              │
                    tasks pile up in flight
                    messages buffer in queue_nodes
                    memory ∝ (production rate − consumption rate) × time
```

Three levers:

```
   1. limiter_node        cap messages past a point in the graph
   2. function_node N     cap concurrent body invocations per node
   3. node_priority_t     prefer critical nodes when workers are busy
```

Part 7.4 (performance tuning) covers general Amdahl analysis; this chapter is
graph-specific flow control.

---

## 6.4.2 Node priorities

> **The API ▸**
>
> ```cpp
> using namespace tbb::flow;
> // higher value → higher scheduling preference
> function_node<int, int> hot(g, unlimited, body, node_priority_t(2));
> function_node<int, int> cold(g, unlimited, body, node_priority_t(1));
> continue_node<continue_msg> crit(g, body, node_priority_t(1));
> // no_priority (0) = default, no bias
> ```
>
> Applies to `function_node`, `continue_node`, `multifunction_node`, `async_node`.
> **`limiter_node` does not support priorities.**

**Under the hood ▸** When a worker searches for the next graph task, nodes with
higher `node_priority_t` win over lower or `no_priority` peers **that are ready
to run**. Priorities are **relative within a graph**, not OS thread priorities.
They do not preempt a running body — they only affect **which ready node** gets
the next task slot.

**Trade-offs ▸** Priorities help when one branch is on the critical path (e.g.,
get partial results to the user before optional analytics finish). They do not
limit memory — pair with `limiter_node` for that.

---

## 6.4.3 limiter_node — token bucket at a graph edge

A `limiter_node` allows at most **`threshold`** messages through until tokens are
returned via its **decrement port**:

```
   src ──▶ [ limiter ≤3 ] ──▶ processor ──▶ ... ──▶ decrement port ──┐
              ▲                                                      │
              └──────────────── token returned ──────────────────────┘
```

> **The API ▸**
>
> ```cpp
> limiter_node<int> lim(g, 3);   // at most 3 in flight past this point
> make_edge(src, lim);
> make_edge(lim, processor);
> make_edge(processor, lim.decrementer());   // return token when done
> ```
>
> When count ≥ threshold, `try_put` on the limiter **rejects** — upstream may
> stall (e.g., `input_node` buffers one item and stops generating).

**Under the hood ▸** Internal counter ∈ [0, threshold]. Each successful forward
increments; each message on `decrementer()` decrements. When decrement drops
below threshold, the limiter tries to pull from a predecessor and forward —
pulling the pipeline without exceeding the cap.

> **Pitfall ▸** Forgetting the **decrement edge** — tokens never return, the
> limiter accepts `threshold` messages once, then blocks forever. Every path
> through the limited section must eventually signal decrement (often the terminal
> node's output wired back).

---

## 6.4.4 Concurrency limits on function_node

Separate from `limiter_node` but complementary:

| Mechanism | Limits | Scope |
|-----------|--------|-------|
| `function_node(g, N, body)` | N concurrent bodies | this node only |
| `limiter_node(g, K)` | K messages past edge | entire subgraph until decrement |
| `serial` | 1 | ordered, thread-unsafe sinks |

Use **`N`** when the body wraps a resource with fixed slots (GPU contexts, file
handles). Use **`limiter_node`** when **message count** in buffers + in-flight
tasks must stay bounded regardless of body duration variance.

---

## 6.4.5 Example: priority + limiter together

Critical path prioritized; in-flight work capped at three:

```cpp
// build: g++ -std=c++17 -O2 graph_limit.cpp -ltbb -o graph_limit
#include <oneapi/tbb/flow_graph.h>
#include <oneapi/tbb/global_control.h>
#include <chrono>
#include <cstdio>
#include <thread>

int main() {
    using namespace tbb;
    using namespace tbb::flow;

    global_control gc(global_control::max_allowed_parallelism, 4);

    graph g;
    const int max_in_flight = 3;

    input_node<int> src(g, [](flow_control& fc) -> int {
        static int n = 1;
        if (n <= 12) return n++;
        fc.stop();
        return 0;
    }, false);

    limiter_node<int> lim(g, max_in_flight);

    function_node<int, int> heavy(g, unlimited,
        [](int x) {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            return x * x;
        },
        node_priority_t(1));   // prioritize compute

    function_node<int, void> log(g, serial,
        [](int x) { std::printf("  result %d\n", x); });

    make_edge(src, lim);
    make_edge(lim, heavy);
    make_edge(heavy, log);
    make_edge(log, lim.decrementer());   // release slot

    src.activate();
    g.wait_for_all();
    return 0;
}
```

Without the limiter, `input_node` would enqueue 12 tasks immediately while `heavy`
sleeps — 12 buffers + 12 concurrent sleeps. With `max_in_flight = 3`, at most
three messages traverse `heavy` at once; `src` stalls when the limiter rejects,
keeping RSS flat.

```
   time ──▶
   src:  * * * | ... stall ... | * * * | ...
   lim:  ≤3 tokens out, tokens return as log finishes
```

**Tuning ▸** Set threshold ≈ desired pipeline depth (often 2–4× worker count for
I/O-bound stages). Wire decrement from the **last** node in the limited region.
Combine `priority_queue_node` (Part 6.2) upstream with `node_priority_t` on
compute for two-level prioritization — queue orders messages; priority biases
which ready node runs first.

---

## Summary

- **`node_priority_t`**: higher values prefer a node's tasks when workers choose
  ready graph work; default `no_priority` = fair.
- **`limiter_node<T>`**: caps messages in flight; **`decrementer()`** port
  returns tokens — mandatory for sustained throughput.
- **`function_node` concurrency `N`**: limits parallel bodies per node; different
  from message-count limiting.
- Combine priorities (critical path first) with limiters (bounded memory) in
  long-running or high-volume graphs.
- Unbounded `unlimited` + fast sources is the primary graph memory foot-gun.

Next: [7.1 — global_control](../07-advanced-performance/01-global-control.md)
