# 4.3 — Reader-Writer Locks

When shared data is **read often** and **written rarely**, an exclusive mutex
forces readers to serialize unnecessarily. **Reader-writer (RW) locks** allow
**multiple concurrent readers** OR **one exclusive writer** — never both at
once. oneTBB provides **`spin_rw_mutex`** (spin-based, low overhead) and
**`queuing_rw_mutex`** (FIFO-fair variant), mirroring the exclusive pair from
[Part 4.2](02-spin-and-queuing-mutex.md).

RW locks help only when the **read critical section is non-trivial** and writes
are infrequent. A nanosecond read still loses to `std::atomic` (Part 4.4).

---

## 4.3.1 Read vs write modes

![Reader-writer lock: multiple readers OR one writer](figures/rw-lock.svg)

```
   readers (shared):     R1 ──┐
                           R2 ──┼──▶  [ data ]  ✓ all see consistent snapshot
                           R3 ──┘

   writer (exclusive):   W  ──────▶  [ data ]  ✗ blocks all readers & writers
```

> **The API ▸**
> ```cpp
> #include <oneapi/tbb/spin_rw_mutex.h>
> #include <oneapi/tbb/queuing_rw_mutex.h>
>
> tbb::spin_rw_mutex rw;
> tbb::spin_rw_mutex::scoped_lock read_lock(rw, false);   // shared read
> tbb::spin_rw_mutex::scoped_lock write_lock(rw, true);   // exclusive write
> ```
> The **`bool` second argument** to `scoped_lock`: `false` = reader,
> `true` = writer. Same pattern for `queuing_rw_mutex`.

Readers do not block each other. A writer waits for existing readers to finish;
new readers block while a writer holds or waits — preventing writer starvation
in typical implementations.

---

## 4.3.2 Upgrade and downgrade

Sometimes a thread starts read-only and discovers it must mutate. RW locks
support **upgrade** (reader → writer) and **downgrade** (writer → reader)
through the active `scoped_lock`:

> **The API ▸**
> ```cpp
> spin_rw_mutex::scoped_lock lock(rw, false);   // start as reader
> if (need_write) {
>     if (!lock.upgrade_to_writer_lock()) {
>         // upgrade failed — another writer won; re-read or retry strategy
>     }
>     mutate();
>     lock.downgrade_to_reader();   // optional: keep read access
> }
> ```

> **Pitfall ▸** **`upgrade_to_writer_lock()` can return `false`.** While you
> held a read lock, another thread may have become a writer or the policy may
> deny upgrade. **Always branch on the return value** — do not assume upgrade
> succeeds. On failure, release, re-acquire as writer, or retry the read path.
> Holding a read lock and blindly mutating after a failed upgrade is a data race.

Downgrade (`downgrade_to_reader()`) releases exclusive mode while keeping
shared access — useful when the write phase ends but you still need to read
under the same lock object.

---

## 4.3.3 When RW locks actually help

```
   ✓ read:write ratio high (e.g. 90:10 or worse)
   ✓ read section scans data, parses config, aggregates — NOT a single load
   ✓ write section is rare but needs atomic visibility to readers

   ✗ "read" is one atomic load          → use std::atomic
   ✗ write frequency rises              → RW lock ≈ exclusive mutex + overhead
   ✗ hold read lock while blocking I/O  → blocks writers and other readers
```

> **Under the hood ▸** Internally a counter tracks active readers; writers
> wait for it to hit zero. Reader entry is cheap (increment); writer entry
> is expensive (drain readers). **`spin_rw_mutex`** spins; **`queuing_rw_mutex`**
> orders waiters fairly — choose like [spin vs queuing mutex](02-spin-and-queuing-mutex.md).

For read-heavy **maps**, consider [concurrent_hash_map `const_accessor`](../03-concurrent-containers/03-concurrent-hash-map.md)
before rolling your own RW lock around `std::map`.

---

## 4.3.4 Example: shared configuration

```cpp
// g++ -std=c++17 -O2 rw_lock_demo.cpp -ltbb
#include <oneapi/tbb/spin_rw_mutex.h>
#include <oneapi/tbb/parallel_for.h>
#include <oneapi/tbb/blocked_range.h>
#include <map>
#include <string>
#include <cstdio>

class Config {
    tbb::spin_rw_mutex rw_;
    std::map<std::string, std::string> kv_;

public:
    std::string get(const std::string& key) const {
        tbb::spin_rw_mutex& m = const_cast<tbb::spin_rw_mutex&>(rw_);
        tbb::spin_rw_mutex::scoped_lock read(m, false);
        auto it = kv_.find(key);
        return it == kv_.end() ? "" : it->second;
    }

    void set(const std::string& key, const std::string& val) {
        tbb::spin_rw_mutex::scoped_lock write(rw_, true);
        kv_[key] = val;
    }

    bool try_bump_counter(const std::string& key) {
        tbb::spin_rw_mutex::scoped_lock lock(rw_, false);
        auto it = kv_.find(key);
        if (it == kv_.end()) return false;
        if (!lock.upgrade_to_writer_lock())
            return false;   // handle failure — e.g. retry or give up
        int n = std::stoi(it->second);
        it->second = std::to_string(n + 1);
        lock.downgrade_to_reader();
        return true;
    }
};

int main() {
    Config cfg;
    cfg.set("host", "127.0.0.1");

    tbb::parallel_for(
        tbb::blocked_range<int>(0, 10'000),
        [&](const tbb::blocked_range<int>& r) {
            for (int i = r.begin(); i != r.end(); ++i)
                (void)cfg.get("host");   // concurrent reads
        });

    std::printf("host=%s\n", cfg.get("host").c_str());
    return 0;
}
```

**Trade-offs ▸**
- ✓ Parallel reads on non-trivial data structures.
- ✗ Writer blocks all traffic; upgrade can fail; more complex than `mutex`;
  wrong when writes are frequent.

**Tuning ▸** Keep read paths **allocation-free** where possible — allocating
under a read lock increases hold time and delays writers. Watch false sharing
on the RW lock word itself ([Part 5.2](../05-memory/02-cache-alignment-false-sharing.md)).

---

## Summary

- **`spin_rw_mutex`** / **`queuing_rw_mutex`**: **`scoped_lock(m, false)`** for
  readers, **`true`** for writer; multiple readers OR one writer.
- **`upgrade_to_writer_lock()`** may return **`false`** — handle failure; never
  mutate on failed upgrade.
- **`downgrade_to_reader()`** drops exclusive mode while keeping read access.
- Use when reads dominate and read work is **non-trivial**; otherwise prefer
  [atomics](04-atomics.md) or concurrent containers.

Next: [4.4 — Atomics](04-atomics.md)
