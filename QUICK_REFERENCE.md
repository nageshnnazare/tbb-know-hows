# TBB Quick Reference Card

## 🚀 Most Common Operations

### Parallel Loop
```cpp
#include <tbb/parallel_for.h>

parallel_for(blocked_range<int>(0, N),
    [&](blocked_range<int> r) {
        for(int i = r.begin(); i != r.end(); ++i)
            process(data[i]);
    });
```

### Parallel Sum
```cpp
#include <tbb/parallel_reduce.h>

double sum = parallel_reduce(
    blocked_range<int>(0, N),
    0.0,
    [&](blocked_range<int> r, double s) {
        for(int i = r.begin(); i != r.end(); ++i)
            s += data[i];
        return s;
    },
    plus<double>()
);
```

### Thread-Safe Vector
```cpp
#include <tbb/concurrent_vector.h>

concurrent_vector<int> results;
parallel_for(..., [&](...) {
    results.push_back(compute());  // Thread-safe!
});
```

### Thread-Safe Queue
```cpp
#include <tbb/concurrent_queue.h>

concurrent_queue<Task> queue;
queue.push(task);           // Producer
Task t;
queue.try_pop(t);           // Consumer
```

### Parallel Sort
```cpp
#include <tbb/parallel_sort.h>

parallel_sort(data.begin(), data.end());
```

### Parallel Invoke
```cpp
#include <tbb/parallel_invoke.h>

parallel_invoke(
    []() { task1(); },
    []() { task2(); },
    []() { task3(); }
);
```

### Thread-Local Storage
```cpp
#include <tbb/enumerable_thread_specific.h>

enumerable_thread_specific<int> local_sum(0);
parallel_for(..., [&](...) {
    local_sum.local() += compute();
});

int total = 0;
for(auto& s : local_sum) total += s;
```

### Mutex
```cpp
#include <tbb/mutex.h>

tbb::mutex mtx;
int shared_counter = 0;

{
    tbb::mutex::scoped_lock lock(mtx);
    shared_counter++;  // Protected
}
```

### Atomic
```cpp
#include <tbb/atomic.h>

tbb::atomic<int> counter = 0;
counter++;  // Lock-free atomic increment
```

## 📏 Grain Size Guidelines

```cpp
// Rule: Each task should take 10-100 microseconds

// Let TBB decide (recommended)
parallel_for(blocked_range<int>(0, N), body);

// Manual grain size
parallel_for(blocked_range<int>(0, N, 1000), body);
                                      // ↑ grain size
```

## 🎯 Common Patterns

### Pattern 1: Map
```cpp
vector<int> input(N), output(N);
parallel_for(blocked_range<int>(0, N),
    [&](auto r) {
        for(int i = r.begin(); i != r.end(); ++i)
            output[i] = f(input[i]);
    });
```

### Pattern 2: Reduce
```cpp
int sum = parallel_reduce(
    blocked_range<int>(0, N), 0,
    [&](auto r, int s) {
        for(int i = r.begin(); i != r.end(); ++i)
            s += data[i];
        return s;
    },
    plus<int>()
);
```

### Pattern 3: Collect Results
```cpp
concurrent_vector<Result> results;
parallel_for(blocked_range<int>(0, N),
    [&](auto r) {
        for(int i = r.begin(); i != r.end(); ++i)
            results.push_back(compute(i));
    });
```

### Pattern 4: Producer-Consumer
```cpp
concurrent_queue<Task> queue;

// Producer
parallel_for(..., [&](...) {
    queue.push(generate_task());
});

// Consumer
parallel_for(..., [&](...) {
    Task t;
    while(queue.try_pop(t))
        process(t);
});
```

## ⚠️ Common Mistakes

### ❌ Data Race
```cpp
int counter = 0;
parallel_for(..., [&](...) {
    counter++;  // DATA RACE!
});
```

### ✅ Fix: Use Atomic
```cpp
tbb::atomic<int> counter = 0;
parallel_for(..., [&](...) {
    counter++;  // Safe
});
```

### ❌ Using std::vector Concurrently
```cpp
vector<int> vec;
parallel_for(..., [&](...) {
    vec.push_back(42);  // DATA RACE!
});
```

### ✅ Fix: Use concurrent_vector
```cpp
concurrent_vector<int> vec;
parallel_for(..., [&](...) {
    vec.push_back(42);  // Safe
});
```

### ❌ Too Fine-Grained
```cpp
parallel_for(blocked_range<int>(0, N),
    [&](auto r) {
        for(int i = r.begin(); i != r.end(); ++i)
            data[i] = i;  // Too simple!
    });
```

### ✅ Fix: Sequential for Simple Operations
```cpp
for(int i = 0; i < N; ++i)
    data[i] = i;
```

## 🔧 Compilation

```bash
# Basic
g++ -std=c++17 file.cpp -ltbb -o program

# Optimized
g++ -std=c++17 -O3 -march=native file.cpp -ltbb -o program

# With TBB malloc
g++ -std=c++17 -O3 file.cpp -ltbb -ltbbmalloc -o program

# Debug
g++ -std=c++17 -g -DTBB_USE_DEBUG file.cpp -ltbb_debug -o program
```

## 📊 Performance Tips

1. **Grain size**: 10-100μs of work per task
2. **Minimize synchronization**: Use thread-local storage
3. **Use concurrent containers**: Avoid manual locking
4. **Profile first**: Measure before optimizing
5. **Avoid false sharing**: Align to cache lines (64 bytes)
6. **Let TBB manage threads**: Don't create threads manually

## 🎓 Learning Order

1. `parallel_for` - Start here!
2. `parallel_reduce` - Aggregations
3. `concurrent_vector` - Thread-safe containers
4. `mutex` / `atomic` - Synchronization
5. `parallel_sort`, `parallel_invoke` - More algorithms
6. `concurrent_queue`, `concurrent_hash_map` - More containers
7. `enumerable_thread_specific` - Thread-local storage
8. Advanced: pipelines, flow graphs, custom tasks

## 🔗 Quick Links

- GitHub: https://github.com/oneapi-src/oneTBB
- Docs: https://spec.oneapi.io/versions/latest/elements/oneTBB/
- This guide: `/tmp/tbb_complete_guide/COMPLETE_TBB_GUIDE.md`

---

**Print this page for quick reference while coding!** 📄

