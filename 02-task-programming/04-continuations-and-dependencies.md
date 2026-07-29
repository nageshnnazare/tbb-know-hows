# 2.4 — Continuations & Dependencies

Many parallel programs are not flat loops — they are **DAGs**: task B waits for tasks A
and C; task D waits for B. The removed legacy **`tbb::task`** API expressed this with
**continuation-passing** and reference counts. Modern oneTBB offers clearer patterns:
**structured fork-join** with `task_group`, and **Flow Graph** for general DAGs (Part 6).

```
   legacy:  parent task waits_for_all children via ref_count + continuation

   modern:  task_group { run; run; wait; }  — tree/DAG approximations
            flow::graph                       — arbitrary DAG (Part 6)
```

---

## 2.4.1 Expressing dependencies

Dependencies mean: **do not start X until Y (and maybe Z) finish**.

| Pattern | Mechanism | Best for |
|---------|-----------|----------|
| Sequential phases | plain C++ then call | A then B, no overlap |
| Fork-join | `task_group` / `parallel_invoke` | Same join for all children |
| Pipeline | `parallel_pipeline` (Part 1.6) | Linear stages |
| General DAG | **Flow Graph** (Part 6) | Join nodes, split, broadcast |

There is no standalone `tbb::continuation` type in modern oneTBB — dependencies are
**composition patterns**, not a separate scheduler feature.

---

## 2.4.2 Modern way: `task_group` + nested run/wait

Simulate "run B after A completes" with nested groups or serial ordering:

```cpp
void run_dag_simple() {
    tbb::task_group g;

    g.run([] {
        load_data();                    // A
    });
    g.wait();                           // barrier: A done

    tbb::task_group g2;
    g2.run([] { transform(); });        // B (needs A)
    g2.run([] { validate(); });         // C (needs A, independent of B)
    g2.wait();

    summarize();                        // D (needs B and C)
}
```

For **dynamic** trees (branching search), spawn children with `run`, join with `wait`, then
spawn the continuation logically by code after `wait`:

```cpp
void search(node* n) {
    if (!n || goal_found) return;

    tbb::task_group g;
    g.run([&] { search(n->left); });
    g.run([&] { search(n->right); });
    g.wait();
    // "continuation" after both subtrees — still on caller's stack
    merge_results(n);
}
```

> **Under the hood ▸** `wait()` is the join edge. Code after `wait()` is your
> continuation — no ref-counting, no `allocate_continuation`.

---

## 2.4.3 Legacy continuation-passing `task` API

Old TBB (removed in oneTBB 2021):

```cpp
// LEGACY — do not use
class MyTask : public tbb::task {
    task* execute() override {
        spawn_children();
        return this;  // continuation scheduling
    }
};
```

Mechanics you may see in old papers:

```
   parent ref_count = 1 + num_children
   each child decrements on finish
   last child wakes continuation
```

**Why it left:** Easy to get ref_counts wrong (use-after-free, deadlock), hard to compose
with exceptions and cancellation. **`task_group`** encodes the same join with RAII.

**Migration map:**

```
   spawn child tasks + continuation  →  task_group::run + wait + code after
   empty_task synchronization      →  task_group::wait or flow::join_node
   complex DAG                       →  flow::graph (Part 6.1)
```

---

## 2.4.4 Blocking-style vs continuation-style

**Blocking-style** (recommended default):

```cpp
tbb::task_group g;
g.run(work);
g.wait();
next_step();   // reads results — clear and debugger-friendly
```

**Continuation-style** (manual, inside tasks):

```cpp
g.run([&] {
    do_part1();
    g.run([&] { do_part2(); });  // nested spawn from worker
});
g.wait();
```

Nested `run` from workers is legal but obscures structure. Prefer **flat spawn + one
wait** per level, or Flow Graph when dependencies are not a tree.

**Trade-offs ▸** Blocking-style stacks are deeper on workers but readable. Flow Graph
inverts control to dataflow — better for many-to-one joins, worse for trivial two-task
forks.

---

## 2.4.5 Recursive decomposition patterns

Regular recursion maps cleanly:

```
   divide problem
        │
   parallel_invoke(left, right)     ← fixed fan-out (Part 1.5)
        or
   task_group run left; run right; wait   ← dynamic (Part 2.1)
        │
   combine (continuation = code after join)
```

Irregular recursion (graph search, adaptive mesh) → **`task_group`** with **`cancel()`**
when a goal is found (Part 2.1).

Scan/reduce dependencies across a **linear** index range → **`parallel_scan`** /
**`parallel_reduce`** (Part 1.2, Part 1.3), not manual continuations.

---

## 2.4.6 Flow Graph — the general DAG tool

When dependencies are **not** a simple tree or pipeline — merge from three producers,
conditional edges, priority — use the **Flow Graph** (Part 6):

```
   [source] ──▶ [process A] ──┐
                              ├──▶ [join] ──▶ [sink]
   [source] ──▶ [process B] ──┘
```

`flow::graph` + `function_node` + `join_node` + `continue_node` express arbitrary
DAGs; the runtime schedules ready nodes when predecessors complete. That is the spiritual
successor to continuation graphs without manual ref-counting.

Forward reference: start with [Part 6.1 — Flow Graph fundamentals](../06-flow-graph/01-flow-graph-intro.md)
when `task_group` join points become unwieldy.

---

## 2.4.7 Example: phased dependencies with task_group

```cpp
// g++ -std=c++17 -O2 dependencies_demo.cpp -ltbb
#include <oneapi/tbb/task_group.h>
#include <cstdio>
#include <vector>

int main() {
    std::vector<int> data;
    int checksum = 0;
    double stats = 0.0;

    // Phase 1: load (A)
    tbb::task_group load;
    load.run([&] {
        data = {1, 2, 3, 4, 5, 6, 7, 8};
    });
    load.wait();

    // Phase 2: parallel B and C after A
    tbb::task_group analyze;
    analyze.run([&] {
        for (int x : data) checksum += x;
    });
    analyze.run([&] {
        for (int x : data) stats += x * 0.1;
    });
    analyze.wait();

    // Phase 3: D depends on B and C
    std::printf("checksum=%d  stats=%.1f  combined=%.1f\n",
                checksum, stats, checksum + stats);
    return 0;
}
```

For overlapping B and C with a single join, one `task_group` with two `run` calls suffices
(as shown). Phases that must be strictly sequential get separate groups or plain serial
code between `wait()` calls.

---

## Summary

- Dependencies = **do not proceed until predecessors finish** — express with joins, not
  shared flags without synchronization.
- **Modern default:** `task_group` (or `parallel_invoke`) + **`wait()`** + code after =
  continuation.
- Legacy **`tbb::task`** continuation-passing is **removed**; migrate to `task_group` or
  Flow Graph.
- **Blocking-style** join at `wait()` is clearer than deep nested spawns.
- **Flow Graph (Part 6)** is the general DAG tool when fork-join is not enough.

Next: [Part 3.1 — concurrent_vector](../03-concurrent-containers/01-concurrent-vector.md)
