# Intel TBB - Complete Learning Guide

## 📚 Table of Contents

1. [Introduction](#introduction)
2. [Core Concepts](#core-concepts)
3. [Parallel Algorithms](#parallel-algorithms)
4. [Task-Based Programming](#task-based-programming)
5. [Concurrent Containers](#concurrent-containers)
6. [Synchronization](#synchronization)
7. [Memory Management](#memory-management)
8. [Flow Graph](#flow-graph)
9. [Advanced Topics](#advanced-topics)
10. [Best Practices](#best-practices)
11. [Common Pitfalls](#common-pitfalls)

---

## 📖 Introduction

### What is TBB?

Intel Threading Building Blocks (TBB) is a C++ template library for **parallel programming** that:

- **Abstracts low-level threading**: No manual thread management
- **Uses task-based parallelism**: Better than thread-based approaches
- **Implements work-stealing**: Automatic load balancing
- **Provides high-level constructs**: Parallel algorithms, concurrent containers
- **Is portable**: Works across Windows, Linux, macOS
- **Scales automatically**: Adapts to available cores

### Why Use TBB?

| Advantage | Description |
|-----------|-------------|
| **Productivity** | Write parallel code faster with high-level abstractions |
| **Performance** | Work-stealing scheduler ensures optimal CPU utilization |
| **Scalability** | Automatically scales from 1 to 1000+ cores |
| **Safety** | Concurrent containers eliminate common threading bugs |
| **Composability** | Nested parallelism works correctly |

---

## 🎯 Core Concepts

### 1. Task-Based Parallelism

**Traditional Threading:**
```cpp
// Manual thread management (error-prone, doesn't scale)
vector<thread> threads;
for(int i = 0; i < num_threads; ++i) {
    threads.emplace_back([i]() { work(i); });
}
for(auto& t : threads) t.join();
```

**TBB Tasks:**
```cpp
// High-level, scalable (TBB manages threads)
parallel_for(blocked_range<int>(0, N),
    [&](blocked_range<int> r) {
        for(int i = r.begin(); i != r.end(); ++i)
            work(i);
    });
```

### 2. Work-Stealing Scheduler

TBB uses a sophisticated **work-stealing scheduler**:

```
Thread 1: [====Task===]  [===Task===]  [=Task=]  (busy)
Thread 2: [==Task==]                                (idle) -> STEAL from Thread 1!
Thread 3: [=====Task=====]  [====Task====]         (busy)
Thread 4: [===Task===]                              (idle) -> STEAL from Thread 3!
```

**Benefits:**
- Idle threads steal work from busy threads
- Automatic load balancing
- Better than static work distribution
- Handles irregular workloads well

### 3. Grain Size

**Grain size** = Amount of work per task

```cpp
parallel_for(blocked_range<int>(0, N, grain_size),  // <-- grain size
    [&](blocked_range<int> r) { /* work */ });
```

**Rules of Thumb:**
- Each task should take **10-100 microseconds**
- Too small → overhead dominates
- Too large → poor load balancing
- Let TBB choose (default) or tune manually

### 4. Partitioners

Control how work is divided:

| Partitioner | Use When | Characteristics |
|-------------|----------|-----------------|
| `auto_partitioner` | Most cases (default) | Automatic grain size, work stealing |
| `static_partitioner` | Uniform work | Fixed division, no stealing |
| `simple_partitioner` | Deterministic results | Simple splitting |
| `affinity_partitioner` | Cache locality important | Remembers assignment |

---

## 🚀 Parallel Algorithms

### parallel_for - Independent Iterations

**Purpose:** Execute loop iterations in parallel

**Syntax:**
```cpp
parallel_for(range, body, [partitioner])
```

**Example:**
```cpp
vector<int> data(1000000);
parallel_for(blocked_range<int>(0, data.size()),
    [&](blocked_range<int> r) {
        for(int i = r.begin(); i != r.end(); ++i) {
            data[i] = expensive_computation(i);
        }
    });
```

**When to Use:**
- ✅ Loop iterations are independent
- ✅ Each iteration does significant work
- ✅ No data dependencies between iterations

**When NOT to Use:**
- ❌ Loop body is too fast (<1μs per iteration)
- ❌ Iterations have dependencies
- ❌ Order matters and can't be changed

### parallel_reduce - Aggregation

**Purpose:** Combine values from a range (sum, min, max, etc.)

**Syntax:**
```cpp
result = parallel_reduce(range, identity, reduce_body, join_body)
```

**Example - Sum:**
```cpp
double sum = parallel_reduce(
    blocked_range<int>(0, N),
    0.0,  // identity
    [&](blocked_range<int> r, double partial) {
        for(int i = r.begin(); i != r.end(); ++i)
            partial += data[i];
        return partial;
    },
    [](double a, double b) { return a + b; }  // join
);
```

**Requirements:**
- Operation must be **associative**: (a ⊕ b) ⊕ c = a ⊕ (b ⊕ c)
- Need an **identity value**: identity ⊕ x = x
- Join operation combines partial results

**Common Reductions:**
```cpp
// Sum
parallel_reduce(range, 0, reduce, plus<int>())

// Product
parallel_reduce(range, 1, reduce, multiplies<int>())

// Min
parallel_reduce(range, INT_MAX, reduce, [](int a, int b) { return min(a,b); })

// Max
parallel_reduce(range, INT_MIN, reduce, [](int a, int b) { return max(a,b); })

// Custom aggregation
parallel_reduce(range, MyStruct(), reduce, join)
```

### parallel_scan - Prefix Sum

**Purpose:** Compute prefix sums (cumulative sums)

**Example:**
```cpp
// Input:  [1, 2, 3, 4, 5]
// Output: [1, 3, 6, 10, 15]  (cumulative sum)

vector<int> input = {1, 2, 3, 4, 5};
vector<int> output(input.size());

parallel_scan(
    blocked_range<int>(0, input.size()),
    0,  // identity
    [&](blocked_range<int> r, int sum, bool is_final) {
        for(int i = r.begin(); i != r.end(); ++i) {
            sum += input[i];
            if(is_final) output[i] = sum;
        }
        return sum;
    },
    [](int a, int b) { return a + b; }
);
```

### parallel_sort - Parallel Sorting

**Purpose:** Sort a range in parallel

**Example:**
```cpp
vector<int> data = {3, 1, 4, 1, 5, 9, 2, 6};
parallel_sort(data.begin(), data.end());
// data = {1, 1, 2, 3, 4, 5, 6, 9}

// With custom comparator
parallel_sort(data.begin(), data.end(), greater<int>());
```

**Performance:**
- O(N log N) like std::sort
- Faster than std::sort for large arrays
- Uses quicksort + insertion sort hybrid

### parallel_invoke - Function Parallelism

**Purpose:** Execute multiple functions in parallel

**Example:**
```cpp
parallel_invoke(
    []() { task1(); },
    []() { task2(); },
    []() { task3(); },
    []() { task4(); }
);
// All tasks execute in parallel, waits for all to complete
```

**Use Cases:**
- Independent function calls
- Divide-and-conquer algorithms
- Pipeline stages

### parallel_pipeline - Streaming Parallelism

**Purpose:** Process items through pipeline stages

**Example:**
```cpp
parallel_pipeline(
    max_tokens,
    make_filter<void, Item*>(filter::serial_in_order,
        [&](flow_control& fc) -> Item* {
            // Stage 1: Generate items
            return next_item();
        })
    &
    make_filter<Item*, Item*>(filter::parallel,
        [](Item* item) -> Item* {
            // Stage 2: Process items
            process(item);
            return item;
        })
    &
    make_filter<Item*, void>(filter::serial_in_order,
        [&](Item* item) {
            // Stage 3: Output items
            output(item);
        })
);
```

---

## 🧵 Concurrent Containers

### concurrent_vector - Dynamic Array

**Features:**
- Thread-safe push_back
- Concurrent element access
- **Never invalidates iterators** (unlike std::vector)
- Uses segmented storage

**Example:**
```cpp
concurrent_vector<int> vec;

// Multiple threads can push_back concurrently
parallel_for(blocked_range<int>(0, N),
    [&](blocked_range<int> r) {
        for(int i = r.begin(); i != r.end(); ++i)
            vec.push_back(i);  // Thread-safe!
    });
```

**Key Methods:**
```cpp
vec.push_back(value);       // Thread-safe append
vec.grow_by(n);             // Atomically add n elements
vec.grow_to_at_least(size); // Ensure minimum size
vec[i];                     // Concurrent access
```

### concurrent_queue - Lock-Free Queue

**Features:**
- Lock-free FIFO queue
- Multiple producers, multiple consumers
- Unbounded capacity
- Linearizable

**Example:**
```cpp
concurrent_queue<Task> queue;

// Producer threads
parallel_for(blocked_range<int>(0, NUM_PRODUCERS),
    [&](blocked_range<int> r) {
        for(int i = r.begin(); i != r.end(); ++i)
            queue.push(Task(i));
    });

// Consumer threads
parallel_for(blocked_range<int>(0, NUM_CONSUMERS),
    [&](blocked_range<int> r) {
        Task task;
        while(queue.try_pop(task))
            process(task);
    });
```

**Key Methods:**
```cpp
queue.push(item);           // Add item
bool queue.try_pop(item);   // Remove item (non-blocking)
bool queue.empty();         // Check if empty
```

### concurrent_hash_map - Thread-Safe Hash Table

**Features:**
- Fine-grained locking
- Concurrent lookups, insertions, deletions
- No iterator invalidation
- Accessor pattern for safe access

**Example:**
```cpp
concurrent_hash_map<string, int> map;

// Insert
map.insert(make_pair("key", 42));

// Lookup with accessor
concurrent_hash_map<string, int>::accessor a;
if(map.find(a, "key")) {
    cout << "Value: " << a->second << endl;
    a->second = 100;  // Modify while locked
}  // Automatically releases lock
```

### concurrent_unordered_map - Modern Hash Map

**Features:**
- Similar to std::unordered_map interface
- Concurrent operations
- Better performance than concurrent_hash_map
- Preferred for new code

### concurrent_bounded_queue - Bounded Queue

**Features:**
- Fixed capacity
- Blocking operations
- FIFO ordering
- Producer-consumer pattern

**Example:**
```cpp
concurrent_bounded_queue<Task> queue;
queue.set_capacity(1000);

// Producer (blocks if full)
queue.push(task);

// Consumer (blocks if empty)
Task t;
queue.pop(t);
```

### concurrent_priority_queue - Thread-Safe Priority Queue

**Features:**
- Concurrent push/pop
- Priority ordering
- No iterator support

**Example:**
```cpp
concurrent_priority_queue<int> pq;
pq.push(5);
pq.push(1);
pq.push(10);

int val;
if(pq.try_pop(val))
    cout << val << endl;  // Prints 10 (highest)
```

---

## 🔒 Synchronization Primitives

### Mutexes

| Mutex Type | Use When | Characteristics |
|------------|----------|-----------------|
| `mutex` | Default choice | Fair, scalable |
| `spin_mutex` | Very short critical sections | Busy-wait, low overhead |
| `queuing_mutex` | Need FIFO fairness | Guaranteed ordering |
| `spin_rw_mutex` | Multiple readers | Read/write lock, spinlock |
| `queuing_rw_mutex` | Multiple readers, fair | Read/write, FIFO |

**Example:**
```cpp
tbb::mutex mtx;
int shared_counter = 0;

parallel_for(blocked_range<int>(0, N),
    [&](blocked_range<int> r) {
        for(int i = r.begin(); i != r.end(); ++i) {
            tbb::mutex::scoped_lock lock(mtx);
            shared_counter++;  // Protected
        }
    });
```

### Atomic Operations

**Example:**
```cpp
tbb::atomic<int> counter = 0;

parallel_for(blocked_range<int>(0, N),
    [&](blocked_range<int> r) {
        for(int i = r.begin(); i != r.end(); ++i)
            counter++;  // Lock-free atomic increment
    });
```

### Reader-Writer Lock

**Example:**
```cpp
tbb::spin_rw_mutex rw_mutex;
int shared_data = 0;

// Readers
tbb::spin_rw_mutex::scoped_lock read_lock(rw_mutex, false);  // read mode
int val = shared_data;

// Writer
tbb::spin_rw_mutex::scoped_lock write_lock(rw_mutex, true);  // write mode
shared_data = 42;
```

---

## 💾 Memory Management

### scalable_allocator - TBB Memory Allocator

**Purpose:** Replace malloc/new for better multithreaded performance

**Example:**
```cpp
// Use with STL containers
vector<int, tbb::scalable_allocator<int>> vec;

// Or allocate directly
int* ptr = tbb::scalable_allocator<int>().allocate(1000);
// ... use ptr ...
tbb::scalable_allocator<int>().deallocate(ptr, 1000);
```

**Benefits:**
- Eliminates malloc contention
- Better scalability with many threads
- Compatible with STL

### cache_aligned_allocator

**Purpose:** Align data to cache lines (avoid false sharing)

**Example:**
```cpp
// Each element on separate cache line
vector<int, tbb::cache_aligned_allocator<int>> data;
```

### enumerable_thread_specific - Thread-Local Storage

**Purpose:** Each thread gets its own copy of data

**Example:**
```cpp
enumerable_thread_specific<int> thread_local_sum(0);

parallel_for(blocked_range<int>(0, N),
    [&](blocked_range<int> r) {
        int& local_sum = thread_local_sum.local();
        for(int i = r.begin(); i != r.end(); ++i)
            local_sum += data[i];
    });

// Combine all thread-local values
int total = 0;
for(auto& local : thread_local_sum)
    total += local;
```

---

## 📊 Performance Best Practices

### 1. Grain Size Tuning

```cpp
// Too small (overhead dominates)
parallel_for(blocked_range<int>(0, N, 1), ...);  // Bad!

// Too large (poor load balancing)
parallel_for(blocked_range<int>(0, N, N), ...);  // Bad!

// Good (10-100μs of work)
parallel_for(blocked_range<int>(0, N, 1000), ...);  // Good!

// Let TBB decide (usually best)
parallel_for(blocked_range<int>(0, N), ...);  // Best!
```

### 2. Minimize Synchronization

```cpp
// Bad: Lock in inner loop
parallel_for(..., [&](auto r) {
    for(int i : r) {
        mutex::scoped_lock lock(mtx);  // Contention!
        result += data[i];
    }
});

// Good: Accumulate locally, lock once
parallel_for(..., [&](auto r) {
    int local_sum = 0;
    for(int i : r)
        local_sum += data[i];
    mutex::scoped_lock lock(mtx);
    result += local_sum;  // One lock per range
});

// Best: Use parallel_reduce (no explicit locking)
int result = parallel_reduce(...);
```

### 3. Avoid False Sharing

```cpp
// Bad: Array elements on same cache line
struct alignas(64) Data {  // Force cache-line alignment
    int counter;
    char padding[60];
};

// Or use cache_aligned_allocator
```

### 4. Use Concurrent Containers

```cpp
// Bad: Manual locking
mutex mtx;
vector<int> results;
parallel_for(..., [&](...) {
    mutex::scoped_lock lock(mtx);
    results.push_back(compute());
});

// Good: Concurrent container
concurrent_vector<int> results;
parallel_for(..., [&](...) {
    results.push_back(compute());  // Lock-free!
});
```

---

## ⚠️ Common Pitfalls

### 1. Data Races

```cpp
// WRONG: Data race!
int counter = 0;
parallel_for(..., [&](...) {
    counter++;  // Multiple threads, no synchronization
});

// RIGHT: Use atomic
tbb::atomic<int> counter = 0;
parallel_for(..., [&](...) {
    counter++;  // Thread-safe
});
```

### 2. Too Fine-Grained Parallelism

```cpp
// WRONG: Overhead >> work
parallel_for(blocked_range<int>(0, N),
    [&](auto r) {
        for(int i : r)
            data[i] = i;  // Too simple, not worth parallelizing
    });

// RIGHT: Sequential for simple operations
for(int i = 0; i < N; ++i)
    data[i] = i;
```

### 3. Nested Locks (Deadlock)

```cpp
// WRONG: Can deadlock
parallel_for(..., [&](...) {
    mutex::scoped_lock lock1(mutex1);
    mutex::scoped_lock lock2(mutex2);  // Deadlock risk!
});

// RIGHT: Always lock in same order
parallel_for(..., [&](...) {
    mutex::scoped_lock lock1(mutex1);  // Always mutex1 first
    mutex::scoped_lock lock2(mutex2);  // Then mutex2
});
```

### 4. Using std::containers Concurrently

```cpp
// WRONG: std::vector not thread-safe
vector<int> vec;
parallel_for(..., [&](...) {
    vec.push_back(42);  // DATA RACE!
});

// RIGHT: Use concurrent_vector
concurrent_vector<int> vec;
parallel_for(..., [&](...) {
    vec.push_back(42);  // Thread-safe
});
```

---

## 🎓 Learning Path

### Beginner (Start Here)
1. ✅ `parallel_for` - Basic parallel loops
2. ✅ `parallel_reduce` - Aggregations
3. ✅ `concurrent_vector` - Thread-safe containers
4. ✅ Basic mutexes

### Intermediate
5. `parallel_sort`, `parallel_invoke`
6. `concurrent_queue`, `concurrent_hash_map`
7. `enumerable_thread_specific`
8. Partitioners and grain size tuning

### Advanced
9. `parallel_pipeline` - Streaming
10. Flow graph programming
11. Custom tasks
12. Performance optimization

---

## 📚 Quick Reference

### Compilation

```bash
# Basic
g++ -std=c++17 program.cpp -ltbb -o program

# Optimized
g++ -std=c++17 -O3 -march=native program.cpp -ltbb -o program

# With TBB malloc
g++ -std=c++17 -O3 program.cpp -ltbb -ltbbmalloc -o program

# Debug
g++ -std=c++17 -g -DTBB_USE_DEBUG program.cpp -ltbb_debug -o program
```

### Common Patterns

```cpp
// Pattern 1: Parallel loop
parallel_for(blocked_range<int>(0, N),
    [&](auto r) { /* work */ });

// Pattern 2: Parallel sum
double sum = parallel_reduce(
    blocked_range<int>(0, N), 0.0,
    [&](auto r, double s) { /* compute */ return s; },
    plus<double>());

// Pattern 3: Concurrent collection
concurrent_vector<Result> results;
parallel_for(..., [&](...) {
    results.push_back(compute());
});

// Pattern 4: Thread-local accumulation
enumerable_thread_specific<int> local_sum(0);
parallel_for(..., [&](...) {
    local_sum.local() += compute();
});
int total = 0;
for(auto& s : local_sum) total += s;
```

---

## 🎯 Summary

### Key Takeaways

1. **Use TBB for CPU-bound parallelism** - Not for I/O
2. **Start with high-level algorithms** - `parallel_for`, `parallel_reduce`
3. **Use concurrent containers** - Eliminate manual locking
4. **Tune grain size** - 10-100μs of work per task
5. **Profile before optimizing** - Measure, don't guess
6. **Avoid fine-grained synchronization** - Use thread-local storage
7. **Let TBB manage threads** - Don't create threads manually
8. **Test with ThreadSanitizer** - Catch data races early

### When to Use TBB

✅ **Use TBB when:**
- CPU-bound computations
- Independent iterations
- Large data processing
- Need thread-safe containers
- Want automatic scaling

❌ **Don't use TBB when:**
- I/O-bound operations
- Single-threaded performance critical
- Real-time constraints
- Very fine-grained tasks (<1μs)

---

**You're now ready to write high-performance parallel code with TBB!** 🚀

For more examples, see the individual `.cpp` files in this guide.

