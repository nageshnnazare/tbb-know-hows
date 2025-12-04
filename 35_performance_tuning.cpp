#define TBB_PREVIEW_GLOBAL_CONTROL 1

/*
 * TBB PERFORMANCE TUNING - Optimization Techniques
 * 
 * COMPREHENSIVE GUIDE TO TBB PERFORMANCE OPTIMIZATION
 * 
 * PERFORMANCE FACTORS:
 * 1. Grain Size: Balance overhead vs load balancing
 * 2. Partitioner Choice: auto, simple, static, affinity
 * 3. Concurrency Control: Thread count, arenas
 * 4. Memory: Allocators, false sharing, cache locality
 * 5. Algorithm Choice: For, reduce, scan, pipeline
 * 6. Data Structures: Concurrent containers
 * 
 * OPTIMIZATION STRATEGIES:
 * - Measure first (profile before optimizing)
 * - Start with defaults
 * - Tune grain size
 * - Choose right partitioner
 * - Avoid false sharing
 * - Use scalable allocators
 * - Minimize synchronization
 * - Prefer lock-free when possible
 * 
 * COMMON PITFALLS:
 * - Too fine-grained parallelism
 * - False sharing
 * - Excessive synchronization
 * - Wrong algorithm choice
 * - Oversubscription
 */

#include <iostream>
#include <thread>
#include <algorithm>
#include <vector>
#include <chrono>
#include <cmath>
#include <atomic>
#include <tbb/parallel_for.h>
#include <tbb/parallel_reduce.h>
#include <tbb/blocked_range.h>
#include <tbb/partitioner.h>
#include <tbb/scalable_allocator.h>
#include <tbb/cache_aligned_allocator.h>
#include <tbb/enumerable_thread_specific.h>
#include <tbb/global_control.h>
#include <tbb/spin_mutex.h>

using namespace std;
using std::atomic;
using namespace tbb;
using namespace chrono;

// ============= Example 1: Grain Size Tuning =============

void example1_grain_size() {
    cout << "\n=== Example 1: Grain Size Tuning ===" << endl;
    
    const int N = 10000000;
    vector<double> data(N, 1.0);
    
    cout << "Testing different grain sizes:" << endl;
    
    // Light work per iteration
    auto light_work = [&](blocked_range<int> r) {
        for(int i = r.begin(); i != r.end(); ++i) {
            data[i] += 1.0;
        }
    };
    
    for(int grain : {100, 1000, 10000, 100000}) {
        auto start = high_resolution_clock::now();
        parallel_for(blocked_range<int>(0, N, grain), light_work);
        auto time = duration_cast<milliseconds>(
            high_resolution_clock::now() - start).count();
        
        cout << "  Grain " << grain << ": " << time << " ms" << endl;
    }
    
    cout << "\nRule of thumb: grain size = 1000-10000 iterations" << endl;
    cout << "More work per iteration → larger grain" << endl;
}

// ============= Example 2: Partitioner Selection =============

void example2_partitioners() {
    cout << "\n=== Example 2: Partitioner Selection ===" << endl;
    
    const int N = 20000000;
    vector<int> data(N);
    
    auto uniform_work = [&](blocked_range<int> r) {
        for(int i = r.begin(); i != r.end(); ++i) {
            data[i] = i * 2;
        }
    };
    
    cout << "Uniform workload:" << endl;
    
    // auto_partitioner
    {
        auto start = high_resolution_clock::now();
        parallel_for(blocked_range<int>(0, N), uniform_work, auto_partitioner());
        auto time = duration_cast<milliseconds>(
            high_resolution_clock::now() - start).count();
        cout << "  auto_partitioner:   " << time << " ms" << endl;
    }
    
    // simple_partitioner (best for uniform)
    {
        auto start = high_resolution_clock::now();
        parallel_for(blocked_range<int>(0, N), uniform_work, simple_partitioner());
        auto time = duration_cast<milliseconds>(
            high_resolution_clock::now() - start).count();
        cout << "  simple_partitioner: " << time << " ms (best)" << endl;
    }
    
    // static_partitioner
    {
        auto start = high_resolution_clock::now();
        parallel_for(blocked_range<int>(0, N), uniform_work, static_partitioner());
        auto time = duration_cast<milliseconds>(
            high_resolution_clock::now() - start).count();
        cout << "  static_partitioner: " << time << " ms" << endl;
    }
}

// ============= Example 3: False Sharing Elimination =============

struct CounterUnaligned {
    std::atomic<long long> value;
};

struct CounterAligned {
    alignas(64) std::atomic<long long> value;
};

void example3_false_sharing() {
    cout << "\n=== Example 3: False Sharing Elimination ===" << endl;
    
    const int N = 50000000;
    const int THREADS = 4;
    
    // Without alignment (false sharing)
    {
        vector<CounterUnaligned> counters(THREADS);
        for(auto& c : counters) c.value = 0;
        
        auto start = high_resolution_clock::now();
        
        parallel_for(blocked_range<int>(0, THREADS),
            [&](blocked_range<int> r) {
                int tid = r.begin();
                for(int i = 0; i < N / THREADS; ++i) {
                    counters[tid].value++;
                }
            }, simple_partitioner());
        
        auto time = duration_cast<milliseconds>(
            high_resolution_clock::now() - start).count();
        
        cout << "Without alignment: " << time << " ms (slow due to false sharing)" << endl;
    }
    
    // With alignment (no false sharing)
    {
        vector<CounterAligned> counters(THREADS);
        for(auto& c : counters) c.value = 0;
        
        auto start = high_resolution_clock::now();
        
        parallel_for(blocked_range<int>(0, THREADS),
            [&](blocked_range<int> r) {
                int tid = r.begin();
                for(int i = 0; i < N / THREADS; ++i) {
                    counters[tid].value++;
                }
            }, simple_partitioner());
        
        auto time = duration_cast<milliseconds>(
            high_resolution_clock::now() - start).count();
        
        cout << "With alignment:    " << time << " ms (fast!)" << endl;
    }
    
    cout << "\nKey: Align thread-local data to cache line (64 bytes)" << endl;
}

// ============= Example 4: Scalable Allocators =============

void example4_allocators() {
    cout << "\n=== Example 4: Scalable Allocators ===" << endl;
    
    const int N = 1000;
    const int SIZE = 10000;
    
    // Standard allocator
    {
        auto start = high_resolution_clock::now();
        
        parallel_for(blocked_range<int>(0, N),
            [&](blocked_range<int> r) {
                for(int i = r.begin(); i != r.end(); ++i) {
                    vector<int> temp(SIZE);
                    temp[0] = i;
                }
            });
        
        auto time = duration_cast<milliseconds>(
            high_resolution_clock::now() - start).count();
        
        cout << "std::allocator:      " << time << " ms" << endl;
    }
    
    // Scalable allocator
    {
        auto start = high_resolution_clock::now();
        
        parallel_for(blocked_range<int>(0, N),
            [&](blocked_range<int> r) {
                for(int i = r.begin(); i != r.end(); ++i) {
                    vector<int, scalable_allocator<int>> temp(SIZE);
                    temp[0] = i;
                }
            });
        
        auto time = duration_cast<milliseconds>(
            high_resolution_clock::now() - start).count();
        
        cout << "scalable_allocator:  " << time << " ms (faster!)" << endl;
    }
    
    cout << "\nKey: Use scalable_allocator for parallel allocations" << endl;
}

// ============= Example 5: Thread-Local Storage =============

void example5_thread_local() {
    cout << "\n=== Example 5: Thread-Local Storage ===" << endl;
    
    const int N = 10000000;
    vector<int> data(N);
    for(int i = 0; i < N; ++i) data[i] = i;
    
    // With locking (slow)
    {
        long long sum = 0;
        tbb::spin_mutex mtx;
        
        auto start = high_resolution_clock::now();
        
        parallel_for(blocked_range<int>(0, N),
            [&](blocked_range<int> r) {
                long long local_sum = 0;
                for(int i = r.begin(); i != r.end(); ++i) {
                    local_sum += data[i];
                }
                tbb::spin_mutex::scoped_lock lock(mtx);
                sum += local_sum;
            });
        
        auto time = duration_cast<milliseconds>(
            high_resolution_clock::now() - start).count();
        
        cout << "With mutex: " << time << " ms" << endl;
    }
    
    // Thread-local (fast)
    {
        enumerable_thread_specific<long long> thread_sums(0);
        
        auto start = high_resolution_clock::now();
        
        parallel_for(blocked_range<int>(0, N),
            [&](blocked_range<int> r) {
                long long& local_sum = thread_sums.local();
                for(int i = r.begin(); i != r.end(); ++i) {
                    local_sum += data[i];
                }
            });
        
        long long sum = thread_sums.combine([](long long a, long long b) {
            return a + b;
        });
        
        auto time = duration_cast<milliseconds>(
            high_resolution_clock::now() - start).count();
        
        cout << "Thread-local: " << time << " ms (faster!)" << endl;
    }
    
    cout << "\nKey: Minimize synchronization with thread-local storage" << endl;
}

// ============= Example 6: Algorithm Selection =============

void example6_algorithms() {
    cout << "\n=== Example 6: Algorithm Selection ===" << endl;
    
    const int N = 100000000;
    
    // parallel_for with manual reduction (slower)
    {
        tbb::atomic<long long> sum;
        sum = 0;
        
        auto start = high_resolution_clock::now();
        
        parallel_for(blocked_range<int>(0, N),
            [&](blocked_range<int> r) {
                long long local = 0;
                for(int i = r.begin(); i != r.end(); ++i) {
                    local += i;
                }
                sum += local;
            });
        
        auto time = duration_cast<milliseconds>(
            high_resolution_clock::now() - start).count();
        
        cout << "parallel_for + atomic: " << time << " ms" << endl;
    }
    
    // parallel_reduce (faster)
    {
        auto start = high_resolution_clock::now();
        
        long long sum = parallel_reduce(
            blocked_range<int>(0, N),
            0LL,
            [](blocked_range<int> r, long long init) {
                for(int i = r.begin(); i != r.end(); ++i) {
                    init += i;
                }
                return init;
            },
            [](long long x, long long y) { return x + y; });
        
        auto time = duration_cast<milliseconds>(
            high_resolution_clock::now() - start).count();
        
        cout << "parallel_reduce:       " << time << " ms (faster!)" << endl;
    }
    
    cout << "\nKey: Use parallel_reduce for reductions, not parallel_for" << endl;
}

// ============= Example 7: Thread Count Optimization =============

void example7_thread_count() {
    cout << "\n=== Example 7: Thread Count Optimization ===" << endl;
    
    const int N = 10000000;
    vector<double> data(N, 1.0);
    
    cout << "Performance with different thread counts:" << endl;
    
    long long baseline = 0;
    
    for(int threads = 1; threads <= thread::hardware_concurrency(); threads *= 2) {
        global_control gc(global_control::max_allowed_parallelism, threads);
        
        auto start = high_resolution_clock::now();
        
        parallel_for(blocked_range<int>(0, N),
            [&](blocked_range<int> r) {
                for(int i = r.begin(); i != r.end(); ++i) {
                    data[i] = sqrt(data[i] + 1.0);
                }
            });
        
        auto time = duration_cast<milliseconds>(
            high_resolution_clock::now() - start).count();
        
        if(threads == 1) baseline = time;
        double speedup = (double)baseline / time;
        double efficiency = speedup / threads * 100;
        
        cout << "  " << threads << " threads: " << time << " ms, "
             << "speedup: " << speedup << "x, "
             << "efficiency: " << efficiency << "%" << endl;
    }
}

int main() {
    cout << "╔════════════════════════════════════════════════════════╗" << endl;
    cout << "║   TBB performance_tuning - Complete Tutorial          ║" << endl;
    cout << "╚════════════════════════════════════════════════════════╝" << endl;
    
    example1_grain_size();
    example2_partitioners();
    example3_false_sharing();
    example4_allocators();
    example5_thread_local();
    example6_algorithms();
    example7_thread_count();
    
    cout << "\n╔════════════════════════════════════════════════════════╗" << endl;
    cout << "║                    KEY TAKEAWAYS                       ║" << endl;
    cout << "╠════════════════════════════════════════════════════════╣" << endl;
    cout << "║                                                        ║" << endl;
    cout << "║  1. Measure first - profile before optimizing         ║" << endl;
    cout << "║  2. Grain size: 1000-10000 iterations typical         ║" << endl;
    cout << "║  3. Eliminate false sharing (64-byte alignment)       ║" << endl;
    cout << "║  4. Use scalable_allocator for parallel allocations   ║" << endl;
    cout << "║  5. Thread-local storage avoids synchronization       ║" << endl;
    cout << "║  6. Choose right algorithm (reduce not for)           ║" << endl;
    cout << "║  7. Default thread count usually optimal              ║" << endl;
    cout << "║                                                        ║" << endl;
    cout << "╠════════════════════════════════════════════════════════╣" << endl;
    cout << "║               OPTIMIZATION CHECKLIST                   ║" << endl;
    cout << "╠════════════════════════════════════════════════════════╣" << endl;
    cout << "║                                                        ║" << endl;
    cout << "║  ☐ Profile to find bottlenecks                        ║" << endl;
    cout << "║  ☐ Check grain size (not too small)                   ║" << endl;
    cout << "║  ☐ Verify no false sharing                            ║" << endl;
    cout << "║  ☐ Use scalable_allocator if allocating               ║" << endl;
    cout << "║  ☐ Minimize synchronization                           ║" << endl;
    cout << "║  ☐ Use thread-local storage                           ║" << endl;
    cout << "║  ☐ Right algorithm (reduce vs for)                    ║" << endl;
    cout << "║  ☐ Consider partitioner choice                        ║" << endl;
    cout << "║  ☐ Cache-aligned per-thread data                      ║" << endl;
    cout << "║  ☐ Avoid oversubscription                             ║" << endl;
    cout << "║                                                        ║" << endl;
    cout << "╠════════════════════════════════════════════════════════╣" << endl;
    cout << "║               PERFORMANCE GUIDELINES                   ║" << endl;
    cout << "╠════════════════════════════════════════════════════════╣" << endl;
    cout << "║                                                        ║" << endl;
    cout << "║  Grain Size:                                           ║" << endl;
    cout << "║    Light work:  10000-100000 iterations               ║" << endl;
    cout << "║    Medium work: 1000-10000 iterations                 ║" << endl;
    cout << "║    Heavy work:  100-1000 iterations                   ║" << endl;
    cout << "║                                                        ║" << endl;
    cout << "║  Partitioner:                                          ║" << endl;
    cout << "║    Uniform work:   simple_partitioner                 ║" << endl;
    cout << "║    Imbalanced:     auto_partitioner                   ║" << endl;
    cout << "║    Determinism:    static_partitioner                 ║" << endl;
    cout << "║    Repeated loops: affinity_partitioner               ║" << endl;
    cout << "║                                                        ║" << endl;
    cout << "║  Synchronization:                                      ║" << endl;
    cout << "║    Best:   No shared state (thread-local)             ║" << endl;
    cout << "║    Good:   Atomic operations                           ║" << endl;
    cout << "║    OK:     spin_mutex (short locks)                   ║" << endl;
    cout << "║    Avoid:  Regular mutex (long locks)                 ║" << endl;
    cout << "║                                                        ║" << endl;
    cout << "║  Memory:                                               ║" << endl;
    cout << "║    Allocations:   scalable_allocator                  ║" << endl;
    cout << "║    Per-thread:    cache_aligned_allocator             ║" << endl;
    cout << "║    Arrays:        Align to 64 bytes                   ║" << endl;
    cout << "║                                                        ║" << endl;
    cout << "╠════════════════════════════════════════════════════════╣" << endl;
    cout << "║               COMMON PITFALLS                          ║" << endl;
    cout << "╠════════════════════════════════════════════════════════╣" << endl;
    cout << "║                                                        ║" << endl;
    cout << "║  ✗ Grain size too small (overhead dominates)          ║" << endl;
    cout << "║  ✗ False sharing (adjacent thread data)               ║" << endl;
    cout << "║  ✗ Excessive synchronization (mutexes)                ║" << endl;
    cout << "║  ✗ Wrong algorithm (for instead of reduce)            ║" << endl;
    cout << "║  ✗ Standard allocator in parallel code                ║" << endl;
    cout << "║  ✗ Oversubscription (too many threads)                ║" << endl;
    cout << "║  ✗ No profiling (guessing bottlenecks)                ║" << endl;
    cout << "║  ✗ Premature optimization                             ║" << endl;
    cout << "║                                                        ║" << endl;
    cout << "╠════════════════════════════════════════════════════════╣" << endl;
    cout << "║               MEASUREMENT & PROFILING                  ║" << endl;
    cout << "╠════════════════════════════════════════════════════════╣" << endl;
    cout << "║                                                        ║" << endl;
    cout << "║  1. Always measure actual performance                  ║" << endl;
    cout << "║  2. Use high_resolution_clock for timing              ║" << endl;
    cout << "║  3. Run multiple iterations (average)                 ║" << endl;
    cout << "║  4. Compare against sequential baseline               ║" << endl;
    cout << "║  5. Calculate speedup and efficiency                  ║" << endl;
    cout << "║  6. Profile with tools (vtune, perf, etc.)            ║" << endl;
    cout << "║  7. Test on target hardware                           ║" << endl;
    cout << "║                                                        ║" << endl;
    cout << "╚════════════════════════════════════════════════════════╝" << endl;
    
    cout << "\n\n";
    cout << "╔════════════════════════════════════════════════════════╗" << endl;
    cout << "║                                                        ║" << endl;
    cout << "║   🎉  CONGRATULATIONS!  🎉                            ║" << endl;
    cout << "║                                                        ║" << endl;
    cout << "║   You've completed all 35 TBB examples!                ║" << endl;
    cout << "║                                                        ║" << endl;
    cout << "║   You now have comprehensive knowledge of:             ║" << endl;
    cout << "║   • Parallel algorithms (for, reduce, scan, sort)      ║" << endl;
    cout << "║   • Task management (groups, arenas, scheduler)        ║" << endl;
    cout << "║   • Flow graphs (dataflow programming)                 ║" << endl;
    cout << "║   • Concurrent containers                              ║" << endl;
    cout << "║   • Synchronization (mutexes, atomics, locks)          ║" << endl;
    cout << "║   • Memory management (allocators, alignment)          ║" << endl;
    cout << "║   • Advanced features (partitioners, control)          ║" << endl;
    cout << "║   • Performance tuning                                 ║" << endl;
    cout << "║                                                        ║" << endl;
    cout << "║   Keep practicing and happy parallel programming!      ║" << endl;
    cout << "║                                                        ║" << endl;
    cout << "╚════════════════════════════════════════════════════════╝" << endl;
    
    return 0;
}
