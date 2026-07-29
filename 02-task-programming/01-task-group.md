# 2.1 — task_group

Parallel algorithms (`parallel_for`, etc.) cover regular loops. **`task_group`** is the
lower-level fork-join primitive when you need **dynamic** task creation — recursive trees,
irregular fan-out, or work discovered at runtime — while still composing on the shared
work-stealing scheduler (Part 0.3).

```
   task_group g;
   g.run(f);   g.run(h);     ← spawn tasks (may run immediately or later)
   g.wait();                 ← block until all tasks finish
```

---

## 2.1.1 Core operations

> **The API ▸**
> ```cpp
> // <oneapi/tbb/task_group.h>
> class task_group {
> public:
>     task_group();
>     template<typename F> void run(F&& f);
>     template<typename F> void run_and_wait(F&& f);
>     void wait();
>     void cancel();
>     bool is_canceling() const;
> };
> ```
> **Semantics:** `run(f)` schedules `f` as a task. `wait()` blocks until all tasks spawned
> from this group (including nested `run` from those tasks) complete. `run_and_wait(f)`
> is equivalent to `run(f); wait();` but may optimize the single-task case.

![task_group: dynamic spawn, structured join at wait()](figures/task-group.svg)

---

## 2.1.2 Structured fork-join

A `task_group` defines a **join scope**: every `run()` before `wait()` must complete
before execution continues past `wait()`.

```cpp
tbb::task_group g;
int x = 0, y = 0;

g.run([&] { x = compute_a(); });
g.run([&] { y = compute_b(); });
g.wait();

// safe: both computes finished
use(x, y);
```

This is structured parallelism — analogous to `parallel_invoke` (Part 1.5) but with
**dynamic** task count.

> **Under the hood ▸** Tasks are lightweight (~tens of ns to schedule). The group tracks
> outstanding tasks; the last completing task unblocks `wait()`. No OS thread per task.

---

## 2.1.3 Cancellation

Call **`cancel()`** on the group (typically from another thread or an early-finishing
task). Workers running tasks from that group should poll **`is_canceling()`** and return
early:

```cpp
tbb::task_group g;

g.run([&] {
    for (int i = 0; i < n; ++i) {
        if (g.is_canceling()) return;
        expensive(i);
    }
});

// elsewhere, on failure:
g.cancel();
g.wait();
```

Cancellation is **cooperative** — TBB does not kill threads. Tasks must poll and exit.
Uncooperative tasks block `wait()` until they finish naturally.

**Trade-offs ▸** Cancellation avoids wasted CPU after a global failure (search found
answer, I/O error). It adds polling overhead in inner loops.

---

## 2.1.4 Exception propagation

If a task throws, oneTBB stores the exception and rethrows it from **`wait()`** (or from
the destructor if still pending — avoid letting `task_group` destruct with running tasks).

```cpp
tbb::task_group g;
g.run([] { throw std::runtime_error("fail"); });

try {
    g.wait();
} catch (const std::exception& e) {
    // handle — other tasks may already be running; cancel if needed
    g.cancel();
}
```

> **Pitfall ▸** Throwing across tasks without catching at `wait()` terminates the program
> if the exception escapes the group's join point. Pair exceptions with **`cancel()`** for
> sibling tasks.

---

## 2.1.5 Recursive Fibonacci / tree example

Illustrative only — real Fibonacci should not be computed this way — but the pattern
matches tree search, backtracking, and irregular divide-and-conquer:

```cpp
// g++ -std=c++17 -O2 task_group_demo.cpp -ltbb
#include <oneapi/tbb/task_group.h>
#include <cstdio>

long long fib_serial(int n) {
    if (n < 2) return n;
    return fib_serial(n - 1) + fib_serial(n - 2);
}

long long fib_parallel(int n) {
    if (n < 20) return fib_serial(n);

    long long x = 0, y = 0;
    tbb::task_group g;

    g.run([&] { x = fib_parallel(n - 1); });
    y = fib_parallel(n - 2);   // reuse caller stack for one branch

    g.wait();
    return x + y;
}

int main() {
    const int n = 35;
    long long r = fib_parallel(n);
    std::printf("fib(%d) = %lld\n", n, r);
    return 0;
}
```

Better tree pattern: spawn **both** children with `run`, keep caller idle for join:

```cpp
long long fib_parallel2(int n) {
    if (n < 20) return fib_serial(n);
    long long x = 0, y = 0;
    tbb::task_group g;
    g.run([&] { x = fib_parallel2(n - 1); });
    g.run([&] { y = fib_parallel2(n - 2); });
    g.wait();
    return x + y;
}
```

Compute one branch on the caller when depth is small to limit task churn (same grainsize
idea as Part 0.4).

---

## 2.1.6 task_group vs parallel algorithms

| Need | Use |
|------|-----|
| Regular index loop | `parallel_for` |
| Fixed 2–N branches | `parallel_invoke` |
| Unknown branch count, recursion | `task_group` |
| Cancellation | `task_group` |
| Streaming stages | `parallel_pipeline` |

---

## Summary

- **`task_group::run(f)`** spawns work; **`wait()`** joins all tasks in the group.
- **`run_and_wait`** combines spawn + join for a single task.
- **`cancel()`** / **`is_canceling()`** provide cooperative cancellation.
- Exceptions propagate from **`wait()`**; use **`cancel()`** to stop siblings.
- Use for **dynamic, irregular** fork-join (trees, search) on the shared scheduler.

Next: [2.2 — task_arena](02-task-arena.md)
