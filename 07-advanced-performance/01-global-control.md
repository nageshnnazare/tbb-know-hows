# 7.1 — global_control

Part 0.2 established that the default worker count comes from
`std::thread::hardware_concurrency()`. Part 2.2's `task_arena` caps parallelism
**locally** for a subtree of work. This chapter covers **`tbb::global_control`** —
the process-wide ceiling that applies to **every** arena and algorithm call while
the object is alive.

---

## 7.1.1 A runtime knob with RAII semantics

`global_control` is not a scheduler you invoke; it is a **scoped policy object**.
Construct it with a parameter type and value; the limit holds until the object is
destroyed. Nesting is allowed — the **innermost** (most restrictive) control wins
for overlapping scopes.

![global_control caps worker count process-wide while the RAII object is alive](figures/global-control.svg)

```
   main()
     │
     ├─ global_control(max_allowed_parallelism, 4)  ──▶  at most 4 workers
     │       │
     │       ├─ parallel_for(...)          uses ≤ 4 workers
     │       ├─ nested parallel_reduce     still ≤ 4 (composable)
     │       └─ library B's parallel_for   also ≤ 4
     │
     └─ gc destroyed  ──▶  default concurrency restored
```

> **The API ▸**
> ```cpp
> #include <oneapi/tbb/global_control.h>
>
> class global_control {
> public:
>     enum parameter {
>         max_allowed_parallelism,  // cap total worker threads (int ≥ 1)
>         thread_stack_size,        // bytes for each worker's stack
>         terminate_on_exception    // preview: abort pool on unhandled exception
>     };
>     global_control(parameter p, size_t value);
>     static size_t active_value(parameter p);  // current effective limit
> };
> ```
> **Semantics:** RAII. Limits are **process-wide** and affect all TBB calls in
> all threads while any `global_control` object setting that parameter is alive.
> Nested controls: the **minimum** `max_allowed_parallelism` among active objects
> applies. Thread-safe to construct/destroy from any thread.

---

## 7.1.2 max_allowed_parallelism — the knob you will use

This parameter caps how many **worker threads** the TBB runtime may create across
**all arenas**. It does **not** create threads on its own — it sets an upper bound
on the pool size the scheduler is allowed to grow to.

```
   hardware_concurrency() = 16
        │
        ▼
   global_control(max_allowed_parallelism, 12)
        │
        ▼
   effective workers ≤ 12   (4 cores left for OS, UI, other processes)
```

Typical use cases:

| Scenario | Why cap globally |
|----------|------------------|
| Leave a core for OS / UI | Desktop apps that must stay responsive |
| Container / VM quota | cgroup limit is 4 CPUs; don't spawn 64 workers |
| Mixed serial + parallel phases | Benchmark at 1, 2, 4, … N threads reproducibly |
| Composing parallel libraries | Two libs each assume "all cores" → oversubscription without a cap |
| Thermal / power envelope | Laptop on battery: limit sustained parallelism |

> **Under the hood ▸** Workers are **long-lived** OS threads created once and
> reused (Part 0.2). Lowering `max_allowed_parallelism` does not necessarily
> destroy excess workers immediately — the runtime may park idle workers rather
> than tear down threads. Raising the limit after workers were created may reuse
> parked workers. The guarantee is on **active parallelism during work**, not on
> instantaneous thread count in `top`.

---

## 7.1.3 thread_stack_size — deep recursion safety

Each worker has its own stack. The default is platform-dependent (often 2–8 MiB).
If your task bodies recurse deeply — a recursive `parallel_for` over a tree, for
example — you may hit stack overflow on the default size.

```cpp
// g++ -std=c++17 -O2 global_control_stack.cpp -ltbb
#include <oneapi/tbb/global_control.h>
#include <oneapi/tbb/parallel_for.h>
#include <cstdio>

int main() {
    // Must be set BEFORE the scheduler creates workers (typically: early in main).
    tbb::global_control stack_ctl(
        tbb::global_control::thread_stack_size,
        8 * 1024 * 1024);  // 8 MiB per worker

    tbb::parallel_for(0, 1, [](int) { /* deep recursive work here */ });
    return 0;
}
```

> **Pitfall ▸** `thread_stack_size` must be established **before** workers are
> spawned. Setting it mid-run after `parallel_for` has already initialized the
> pool may have no effect. Put stack-size controls at the top of `main()` or in
> static initialization before any TBB call.

---

## 7.1.4 global_control vs task_arena — global ceiling vs local budget

These solve different problems and compose cleanly:

```
   PROCESS-WIDE                         LOCAL (per call site)
   ─────────────                        ─────────────────────
   global_control                       task_arena(concurrency_limit)
   "never exceed 8 workers total"       "this subgraph uses at most 4 slots"
        │                                        │
        └──────────── both active ──────────────┘
              effective = min(global, arena, hardware)
```

| | `global_control` | `task_arena` |
|---|------------------|--------------|
| Scope | entire process | one arena / one `execute()` |
| Primary use | cap total workers, stack size | isolate workloads, prioritize, limit a phase |
| Nesting | innermost minimum wins | per-arena concurrency |
| See also | this chapter | Part 2.2 |

**Trade-offs ▸** `global_control` is blunt but simple — one line caps everything.
`task_arena` is precise — you can run a 4-thread arena and an 8-thread arena
concurrently on disjoint work, as long as their sum respects the global cap. For
"I/O isolation" or "this library gets 2 threads," prefer `task_arena`; for "this
process must never use more than N cores," use `global_control`.

---

## 7.1.5 A complete example: leave a core, measure scaling

```cpp
// g++ -std=c++17 -O2 global_control_demo.cpp -ltbb
#include <oneapi/tbb/global_control.h>
#include <oneapi/tbb/parallel_for.h>
#include <oneapi/tbb/blocked_range.h>
#include <oneapi/tbb/task_arena.h>
#include <chrono>
#include <cstdio>
#include <thread>
#include <vector>

int main() {
    const int hw = static_cast<int>(std::thread::hardware_concurrency());
    const int reserve = 1;  // leave one core for OS / UI
    const int cap = hw > reserve ? hw - reserve : 1;

    std::printf("hardware_concurrency=%d  cap=%d\n", hw, cap);

    std::vector<double> a(50'000'000, 1.0);

    auto timed_for = [&](const char* label) {
        auto t0 = std::chrono::steady_clock::now();
        tbb::parallel_for(
            tbb::blocked_range<std::size_t>(0, a.size()),
            [&](const tbb::blocked_range<std::size_t>& r) {
                for (std::size_t i = r.begin(); i != r.end(); ++i)
                    a[i] = a[i] * 1.000001 + 0.000001;
            });
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - t0).count();
        std::printf("%s: %lld ms\n", label, static_cast<long long>(ms));
    };

    timed_for("uncapped");

    {
        tbb::global_control gc(
            tbb::global_control::max_allowed_parallelism, cap);
        std::printf("active max_allowed_parallelism=%zu\n",
            tbb::global_control::active_value(
                tbb::global_control::max_allowed_parallelism));
        timed_for("global_control capped");
    }

    // task_arena can further limit a subtree — still bounded by global cap
    tbb::task_arena arena(std::min(4, cap));
    arena.execute([&] { timed_for("arena(4) inside restored global default"); });

    return 0;
}
```

The capped run should leave headroom on a loaded machine. Nested `global_control`
objects take the minimum: an outer `{ gc(..., 8) { inner gc(..., 2) { ... } } }`
block runs with at most 2 workers inside the inner scope.

> **Tuning ▸** For scaling studies, sweep `max_allowed_parallelism` from 1 to N
> and plot speedup (Part 7.4). Place one `global_control` at the outermost scope
> of each benchmark iteration — not inside the timed region — to avoid measuring
> construction overhead.

---

## Summary

- `tbb::global_control` is an **RAII process-wide policy**; the limit holds while
  the object lives and applies to all TBB algorithms and arenas.
- **`max_allowed_parallelism`** caps total worker threads — use it to reserve cores
  for the OS/UI, respect container quotas, or prevent library composition from
  oversubscribing.
- **`thread_stack_size`** sets worker stack depth; set it **before** the pool
  initializes if tasks recurse deeply.
- Nested controls compose by **minimum**; query `active_value()` to see the
  effective limit.
- Prefer **`global_control`** for a global ceiling; prefer **`task_arena`** (Part
  2.2) for local, per-workload isolation — they compose via `min(global, arena)`.

Next: [7.2 — task_scheduler_observer](02-task-scheduler-observer.md)
