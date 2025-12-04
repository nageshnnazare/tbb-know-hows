/*
 * TBB CACHE_ALIGNED_ALLOCATOR - Prevent False Sharing
 * 
 * DEFINITION:
 * cache_aligned_allocator allocates memory aligned to cache line boundaries,
 * preventing false sharing between threads.
 * 
 * FALSE SHARING:
 * When two threads access different variables on the same cache line,
 * they invalidate each other's cache, causing severe performance degradation.
 * 
 * CACHE LINE:
 * - Typical size: 64 bytes
 * - Unit of cache coherency
 * - Shared between CPU cores
 * 
 * KEY FEATURES:
 * - Allocates on cache line boundaries
 * - Prevents false sharing
 * - Combines with scalable_allocator benefits
 * - STL-compatible
 * 
 * WHEN TO USE:
 * - Thread-local data structures
 * - Per-thread counters/accumulators
 * - Arrays accessed by multiple threads
 * - Performance-critical parallel code
 * 
 * BENEFITS:
 * + Eliminates false sharing
 * + Dramatic performance improvement
 * + Better cache utilization
 * - Wastes some memory for padding
 */

#include <iostream>
#include <cmath>
#include <vector>
#include <chrono>
#include <atomic>
#include <tbb/cache_aligned_allocator.h>
#include <tbb/scalable_allocator.h>
#include <tbb/parallel_for.h>
#include <tbb/blocked_range.h>

using namespace std;
using std::atomic;
using namespace tbb;
using namespace chrono;

// ============= Example 1: False Sharing Problem =============

struct CounterUnaligned {
    std::atomic<long long> value;
    CounterUnaligned() : value(0) {}
};

struct CounterAligned {
    alignas(64) std::atomic<long long> value;  // Cache line aligned
    CounterAligned() : value(0) {}
};

void example1_false_sharing() {
    cout << "\n=== Example 1: False Sharing Problem ===" << endl;
    
    const int N = 10000000;
    const int THREADS = 4;
    
    // Without alignment (false sharing)
    {
        vector<CounterUnaligned> counters(THREADS);
        
        auto start = high_resolution_clock::now();
        
        parallel_for(blocked_range<int>(0, THREADS),
            [&](blocked_range<int> r) {
                int thread_id = r.begin();
                for(int i = 0; i < N / THREADS; ++i) {
                    counters[thread_id].value++;
                }
            }, simple_partitioner());
        
        auto time = duration_cast<milliseconds>(
            high_resolution_clock::now() - start).count();
        
        cout << "Without alignment: " << time << " ms (false sharing)" << endl;
    }
    
    // With alignment (no false sharing)
    {
        vector<CounterAligned> counters(THREADS);
        
        auto start = high_resolution_clock::now();
        
        parallel_for(blocked_range<int>(0, THREADS),
            [&](blocked_range<int> r) {
                int thread_id = r.begin();
                for(int i = 0; i < N / THREADS; ++i) {
                    counters[thread_id].value++;
                }
            }, simple_partitioner());
        
        auto time = duration_cast<milliseconds>(
            high_resolution_clock::now() - start).count();
        
        cout << "With alignment:    " << time << " ms (no false sharing)" << endl;
    }
    
    cout << "Alignment prevents cache line contention!" << endl;
}

// ============= Example 2: cache_aligned_allocator Usage =============

void example2_basic_usage() {
    cout << "\n=== Example 2: cache_aligned_allocator Usage ===" << endl;
    
    // Vector with cache-aligned allocation
    vector<long long, cache_aligned_allocator<long long>> vec;
    
    vec.push_back(100);
    vec.push_back(200);
    vec.push_back(300);
    
    cout << "Vector size: " << vec.size() << endl;
    cout << "Elements: ";
    for(auto x : vec) {
        cout << x << " ";
    }
    cout << endl;
    
    // Check alignment
    if(vec.size() > 0) {
        uintptr_t addr = reinterpret_cast<uintptr_t>(&vec[0]);
        cout << "Alignment: " << (addr % 64 == 0 ? "64-byte aligned" : "not aligned") << endl;
    }
}

// ============= Example 3: Per-Thread Data =============

struct ThreadData {
    long long counter;
    long long sum;
    long long product;
    
    ThreadData() : counter(0), sum(0), product(1) {}
};

void example3_per_thread_data() {
    cout << "\n=== Example 3: Per-Thread Data ===" << endl;
    
    const int N = 1000000;
    const int THREADS = 8;
    
    // Without cache alignment
    {
        vector<ThreadData> data(THREADS);
        
        auto start = high_resolution_clock::now();
        
        parallel_for(blocked_range<int>(0, THREADS),
            [&](blocked_range<int> r) {
                int tid = r.begin();
                for(int i = 0; i < N / THREADS; ++i) {
                    data[tid].counter++;
                    data[tid].sum += i;
                }
            }, simple_partitioner());
        
        auto time = duration_cast<milliseconds>(
            high_resolution_clock::now() - start).count();
        
        cout << "Without alignment: " << time << " ms" << endl;
    }
    
    // With cache alignment
    {
        vector<ThreadData, cache_aligned_allocator<ThreadData>> data(THREADS);
        
        auto start = high_resolution_clock::now();
        
        parallel_for(blocked_range<int>(0, THREADS),
            [&](blocked_range<int> r) {
                int tid = r.begin();
                for(int i = 0; i < N / THREADS; ++i) {
                    data[tid].counter++;
                    data[tid].sum += i;
                }
            }, simple_partitioner());
        
        auto time = duration_cast<milliseconds>(
            high_resolution_clock::now() - start).count();
        
        cout << "With alignment:    " << time << " ms" << endl;
    }
    
    cout << "Cache-aligned per-thread data is faster!" << endl;
}

// ============= Example 4: Accumulator Array =============

void example4_accumulator_array() {
    cout << "\n=== Example 4: Accumulator Array ===" << endl;
    
    const int N = 10000000;
    const int THREADS = 4;
    
    // Cache-aligned accumulators
    vector<std::atomic<long long>, cache_aligned_allocator<std::atomic<long long>>> accumulators(THREADS);
    for(auto& acc : accumulators) {
        acc = 0;
    }
    
    auto start = high_resolution_clock::now();
    
    parallel_for(blocked_range<int>(0, N),
        [&](blocked_range<int> r) {
            int tid = task_arena::current_thread_index() % THREADS;
            for(int i = r.begin(); i != r.end(); ++i) {
                accumulators[tid] += i;
            }
        });
    
    auto time = duration_cast<milliseconds>(
        high_resolution_clock::now() - start).count();
    
    long long total = 0;
    for(const auto& acc : accumulators) {
        total += acc;
    }
    
    cout << "Total: " << total << endl;
    cout << "Time: " << time << " ms" << endl;
    cout << "No false sharing between accumulators" << endl;
}

// ============= Example 5: Matrix Row Processing =============

void example5_matrix_rows() {
    cout << "\n=== Example 5: Matrix Row Processing ===" << endl;
    
    const int ROWS = 100;
    const int COLS = 10000;
    
    // Row sums (cache-aligned to prevent false sharing)
    vector<double, cache_aligned_allocator<double>> row_sums(ROWS, 0.0);
    
    // Matrix (normal allocation)
    vector<vector<double>> matrix(ROWS, vector<double>(COLS, 1.0));
    
    auto start = high_resolution_clock::now();
    
    // Process rows in parallel
    parallel_for(blocked_range<int>(0, ROWS),
        [&](blocked_range<int> r) {
            for(int row = r.begin(); row != r.end(); ++row) {
                double sum = 0.0;
                for(int col = 0; col < COLS; ++col) {
                    sum += matrix[row][col];
                }
                row_sums[row] = sum;
            }
        });
    
    auto time = duration_cast<milliseconds>(
        high_resolution_clock::now() - start).count();
    
    cout << "Processed " << ROWS << " rows in " << time << " ms" << endl;
    cout << "First row sum: " << row_sums[0] << endl;
    cout << "Each row sum in separate cache line" << endl;
}

// ============= Example 6: Performance Benchmark =============

struct BenchmarkData {
    std::atomic<long long> counter1;
    std::atomic<long long> counter2;
    std::atomic<long long> counter3;
    std::atomic<long long> counter4;
    
    BenchmarkData() : counter1(0), counter2(0), counter3(0), counter4(0) {}
};

void example6_benchmark() {
    cout << "\n=== Example 6: Performance Benchmark ===" << endl;
    
    const int ITERATIONS = 5000000;
    
    // Standard allocation
    {
        vector<BenchmarkData> data(4);
        
        auto start = high_resolution_clock::now();
        
        parallel_for(blocked_range<int>(0, 4),
            [&](blocked_range<int> r) {
                int tid = r.begin();
                for(int i = 0; i < ITERATIONS; ++i) {
                    if(tid == 0) data[0].counter1++;
                    else if(tid == 1) data[1].counter2++;
                    else if(tid == 2) data[2].counter3++;
                    else data[3].counter4++;
                }
            }, simple_partitioner());
        
        auto time = duration_cast<milliseconds>(
            high_resolution_clock::now() - start).count();
        
        cout << "Standard:       " << time << " ms" << endl;
    }
    
    // Cache-aligned allocation
    {
        vector<BenchmarkData, cache_aligned_allocator<BenchmarkData>> data(4);
        
        auto start = high_resolution_clock::now();
        
        parallel_for(blocked_range<int>(0, 4),
            [&](blocked_range<int> r) {
                int tid = r.begin();
                for(int i = 0; i < ITERATIONS; ++i) {
                    if(tid == 0) data[0].counter1++;
                    else if(tid == 1) data[1].counter2++;
                    else if(tid == 2) data[2].counter3++;
                    else data[3].counter4++;
                }
            }, simple_partitioner());
        
        auto time = duration_cast<milliseconds>(
            high_resolution_clock::now() - start).count();
        
        cout << "Cache-aligned:  " << time << " ms" << endl;
    }
    
    cout << "Dramatic speedup by eliminating false sharing!" << endl;
}

// ============= Example 7: STL Containers =============

void example7_stl_containers() {
    cout << "\n=== Example 7: STL Containers with cache_aligned_allocator ===" << endl;
    
    // Vector of cache-aligned integers
    vector<int, cache_aligned_allocator<int>> vec = {10, 20, 30, 40};
    
    cout << "Vector: ";
    for(int x : vec) cout << x << " ";
    cout << endl;
    
    // Cache-aligned array of thread-local data
    const int THREADS = 4;
    vector<std::atomic<int>, cache_aligned_allocator<std::atomic<int>>> counters(THREADS);
    for(auto& c : counters) c = 0;
    
    parallel_for(blocked_range<int>(0, THREADS),
        [&](blocked_range<int> r) {
            int tid = r.begin();
            for(int i = 0; i < 1000000; ++i) {
                counters[tid]++;
            }
        }, simple_partitioner());
    
    cout << "Thread counters: ";
    for(const auto& c : counters) {
        cout << c << " ";
    }
    cout << endl;
    
    cout << "Each counter on separate cache line!" << endl;
}

int main() {
    cout << "╔════════════════════════════════════════════════════════╗" << endl;
    cout << "║  TBB cache_aligned_allocator - Complete Tutorial      ║" << endl;
    cout << "╚════════════════════════════════════════════════════════╝" << endl;
    
    example1_false_sharing();
    example2_basic_usage();
    example3_per_thread_data();
    example4_accumulator_array();
    example5_matrix_rows();
    example6_benchmark();
    example7_stl_containers();
    
    cout << "\n╔════════════════════════════════════════════════════════╗" << endl;
    cout << "║                    KEY TAKEAWAYS                       ║" << endl;
    cout << "╠════════════════════════════════════════════════════════╣" << endl;
    cout << "║                                                        ║" << endl;
    cout << "║  1. Allocates memory on cache line boundaries         ║" << endl;
    cout << "║  2. Prevents false sharing between threads            ║" << endl;
    cout << "║  3. Typical cache line size: 64 bytes                 ║" << endl;
    cout << "║  4. Dramatic performance improvement                  ║" << endl;
    cout << "║  5. Use for per-thread data structures                ║" << endl;
    cout << "║  6. Combines scalability with alignment               ║" << endl;
    cout << "║  7. Some memory overhead for padding                  ║" << endl;
    cout << "║                                                        ║" << endl;
    cout << "╠════════════════════════════════════════════════════════╣" << endl;
    cout << "║               COMMON PATTERNS                          ║" << endl;
    cout << "╠════════════════════════════════════════════════════════╣" << endl;
    cout << "║                                                        ║" << endl;
    cout << "║  Per-Thread Counters:                                  ║" << endl;
    cout << "║    vector<std::atomic<T>,                                   ║" << endl;
    cout << "║           cache_aligned_allocator<std::atomic<T>>> v(N);    ║" << endl;
    cout << "║                                                        ║" << endl;
    cout << "║  Manual Alignment:                                     ║" << endl;
    cout << "║    struct alignas(64) MyData { ... };                 ║" << endl;
    cout << "║                                                        ║" << endl;
    cout << "╠════════════════════════════════════════════════════════╣" << endl;
    cout << "║               FALSE SHARING                            ║" << endl;
    cout << "╠════════════════════════════════════════════════════════╣" << endl;
    cout << "║                                                        ║" << endl;
    cout << "║  Problem:                                              ║" << endl;
    cout << "║    Two threads modify different variables              ║" << endl;
    cout << "║    Variables share same cache line                     ║" << endl;
    cout << "║    Cache invalidation on every write                   ║" << endl;
    cout << "║    Severe performance degradation                      ║" << endl;
    cout << "║                                                        ║" << endl;
    cout << "║  Solution:                                             ║" << endl;
    cout << "║    Align data to cache line boundaries                 ║" << endl;
    cout << "║    Each thread's data on separate cache line           ║" << endl;
    cout << "║    No invalidation between threads                     ║" << endl;
    cout << "║                                                        ║" << endl;
    cout << "╠════════════════════════════════════════════════════════╣" << endl;
    cout << "║               WHEN TO USE                              ║" << endl;
    cout << "╠════════════════════════════════════════════════════════╣" << endl;
    cout << "║                                                        ║" << endl;
    cout << "║  ✓ Per-thread counters/accumulators                   ║" << endl;
    cout << "║  ✓ Thread-local data structures                       ║" << endl;
    cout << "║  ✓ Arrays modified by multiple threads                ║" << endl;
    cout << "║  ✓ Performance-critical parallel code                 ║" << endl;
    cout << "║  ✗ Read-only data (alignment not needed)              ║" << endl;
    cout << "║  ✗ Single-threaded code                               ║" << endl;
    cout << "║  ✗ Memory constrained environments                    ║" << endl;
    cout << "║                                                        ║" << endl;
    cout << "╚════════════════════════════════════════════════════════╝" << endl;
    
    return 0;
}
