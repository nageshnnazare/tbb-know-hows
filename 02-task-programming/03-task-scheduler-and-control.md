# 2.3 — The Task Scheduler & Control

Modern oneTBB **auto-initializes** the task scheduler. There is no required
`main()`-time setup, no mandatory shutdown call, and **`tbb::task_scheduler_init` is
removed**. Understanding lazy init and the replacement control knobs prevents cargo-cult
legacy code and tells you when **`task_arena`** (Part 2.2) is the right lever.

```
   first TBB call in process
        │
        ▼
   scheduler starts (worker threads, default arena, allocator hooks)
        │
        ▼
   parallel algorithms / task_group / ... until process exit
```

---

## 2.3.1 No explicit init in modern oneTBB

In TBB ≤ 2020, many codebases wrote:

```cpp
// LEGACY — removed in oneTBB 2021+
#include <tbb/task_scheduler_init.h>
tbb::task_scheduler_init init(tbb::task_scheduler_init::default_num_threads());
```

That class conflated **thread-pool size**, **arena slots**, and **scheduler lifetime** in
one RAII object. oneTBB split those concerns:

| Old `task_scheduler_init` use | Modern replacement |
|-------------------------------|-------------------|
| Query default thread count | `tbb::info::default_concurrency()` |
| Limit process-wide parallelism | `tbb::global_control` (Part 7.1) |
| Limit parallelism in a scope | `tbb::task_arena(n)` + `execute` |
| Force scheduler shutdown | `task_scheduler_handle` (advanced; rarely needed) |

> **The API ▸**
> ```cpp
> // <oneapi/tbb/info.h>
> namespace tbb::info {
>     int default_concurrency();   // hardware threads TBB would use by default
> }
> ```
> Callable before or after scheduler init. Replaces
> `task_scheduler_init::default_num_threads()`.

---

## 2.3.2 How initialization and teardown work

1. **Lazy init:** The first call that needs the scheduler (e.g. `parallel_for`,
   `task_group::run`) creates the worker thread pool and default arena.
2. **Workers:** Typically one worker per hardware thread, subject to
   `global_control::max_allowed_parallelism`.
3. **Teardown:** Workers terminate when the library unloads (process exit or
   `dlclose`). No explicit `init.terminate()` in normal apps.

```
   thread A: parallel_for  ──▶ creates pool if absent
   thread B: parallel_for  ──▶ joins existing pool
   main returns            ──▶ static dtors; workers stop
```

**Pitfall ▸** Running TBB from **`dll unload`** or after **`main`** destructors may race
teardown. Finish TBB work before static destruction order kills globals your tasks use.

---

## 2.3.3 `global_control` preview

Process-wide limits use RAII **`global_control`** (full treatment in Part 7.1):

```cpp
#include <oneapi/tbb/global_control.h>

tbb::global_control limit(
    tbb::global_control::max_allowed_parallelism,
    4);

// entire process: at most 4 TBB workers until `limit` is destroyed
tbb::parallel_for(0, n, [](int i) { work(i); });
```

Other parameters include **`thread_stack_size`** and **`terminate`** behavior on unhandled
task exceptions. Nested `global_control` objects take the **most restrictive** active
value.

---

## 2.3.4 Legacy `tbb::task` API — what changed

Pre-oneTBB low-level API ( **removed** ):

```
   tbb::task                    base class for manual task trees
   task::allocate_root / spawn / wait_for_all
   continuation passing       set ref_count, spawn child, wait
   tbb::empty_task            synchronization nodes
```

**Removed because:** Error-prone ref-counting, easy leaks, poor composability with
high-level algorithms. Replacements:

| Legacy pattern | Modern oneTBB |
|----------------|---------------|
| Manual task tree | `task_group` (Part 2.1) |
| Continuation after children | nested `run` + `wait`, or Flow Graph (Part 6) |
| `parallel_for` | unchanged conceptually, same entry points |
| Scheduler init | automatic + `global_control` / `task_arena` |

If you maintain old code, rewrite `task` hierarchies as **`task_group`** or **`parallel_*`**
first; only then tackle Flow Graph for DAG dependencies (Part 2.4, Part 6).

---

## 2.3.5 `info::default_concurrency()`

```cpp
#include <oneapi/tbb/info.h>
#include <cstdio>

int main() {
    std::printf("TBB default workers: %d\n", tbb::info::default_concurrency());
    return 0;
}
```

Use this for sizing shard arrays, not for spawning `std::thread` — TBB already maps
workers to cores. **`this_task_arena::max_concurrency()`** (Part 2.2) reflects the
**current** arena cap, which may differ inside `task_arena(2).execute`.

---

## 2.3.6 When explicit arenas beat global init

Prefer **`task_arena`** when:

```
   ✓  Two subsystems need different caps at the same time
   ✓  Nested foreign parallel runtime (OpenMP inside TBB) needs isolation
   ✓  Testing with arena(1) for deterministic serial behavior
   ✓  Background batch vs foreground UI on different OS threads

   ✗  Single app-wide "use 4 threads everywhere" → global_control is simpler
   ✗  "Initialize TBB in main" → unnecessary; just call parallel algorithms
```

**Trade-offs ▸** `global_control` is coarse (whole process). `task_arena` is fine-grained
(per call stack / thread) but requires discipline about where `execute` boundaries sit.

---

## 2.3.7 Minimal modern program

```cpp
// g++ -std=c++17 -O2 scheduler_demo.cpp -ltbb
#include <oneapi/tbb/parallel_for.h>
#include <oneapi/tbb/task_arena.h>
#include <oneapi/tbb/info.h>
#include <oneapi/tbb/global_control.h>
#include <cstdio>

int main() {
    std::printf("hardware default: %d\n", tbb::info::default_concurrency());

    tbb::global_control gc(tbb::global_control::max_allowed_parallelism, 4);

    tbb::parallel_for(0, 100, [](int i) {
        if (i == 0)
            std::printf("inside loop, arena max = %d\n",
                        tbb::this_task_arena::max_concurrency());
    });
    return 0;
}
```

No init object. Scheduler appeared on first `parallel_for`.

---

## Summary

- Modern oneTBB **auto-initializes**; **`task_scheduler_init` is gone**.
- Query defaults with **`info::default_concurrency()`**; cap process-wide with
  **`global_control`** (Part 7.1).
- Scope limits and isolation → **`task_arena`** (Part 2.2).
- Legacy **`tbb::task`** manual trees → **`task_group`** or Flow Graph.
- Explicit init/shutdown is the exception, not the rule.

Next: [2.4 — Continuations & dependencies](04-continuations-and-dependencies.md)
