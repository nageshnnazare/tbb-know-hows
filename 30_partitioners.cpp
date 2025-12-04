/*
 * TBB PARTITIONERS - Control Work Distribution
 * 
 * DEFINITION:
 * Partitioners control how iteration spaces are divided among threads.
 * They balance between load balancing and cache efficiency.
 * 
 * PARTITIONER TYPES:
 * 1. auto_partitioner (default):
 *    - Automatic load balancing
 *    - Dynamic work stealing
 *    - Best general-purpose choice
 * 
 * 2. simple_partitioner:
 *    - Divides range into equal chunks
 *    - One chunk per thread
 *    - No work stealing
 *    - Best for uniform workloads
 * 
 * 3. static_partitioner:
 *    - Deterministic partitioning
 *    - Cache-friendly
 *    - Reproducible results
 * 
 * 4. affinity_partitioner:
 *    - Cache affinity optimization
 *    - Reuses thread-to-range mapping
 *    - Best for repeated parallel loops
 * 
 * WHEN TO USE EACH:
 * - auto_partitioner: Default, unknown workload
 * - simple_partitioner: Uniform work, minimize overhead
 * - static_partitioner: Need deterministic behavior
 * - affinity_partitioner: Repeated loops on same data
 */

#include <iostream>
#include <vector>
#include <chrono>
#include <cmath>
#include <tbb/parallel_for.h>
#include <tbb/blocked_range.h>
#include <tbb/partitioner.h>
#include <tbb/spin_mutex.h>

using namespace std;
using namespace tbb;
using namespace chrono;

// ============= Example 1: auto_partitioner (Default) =============

void example1_auto_partitioner() {
    cout << "\n=== Example 1: auto_partitioner (Default) ===" << endl;
    
    const int N = 10000000;
    vector<double> data(N, 1.0);
    double sum = 0.0;
    tbb::spin_mutex mtx;
    
    auto start = high_resolution_clock::now();
    
    parallel_for(blocked_range<int>(0, N),
        [&](blocked_range<int> r) {
            double local_sum = 0.0;
            for(int i = r.begin(); i != r.end(); ++i) {
                local_sum += data[i];
            }
            tbb::spin_mutex::scoped_lock lock(mtx);
            sum += local_sum;
        },
        auto_partitioner());  // Automatic load balancing
    
    auto time = duration_cast<milliseconds>(
        high_resolution_clock::now() - start).count();
    
    cout << "Sum: " << sum << endl;
    cout << "Time: " << time << " ms" << endl;
    cout << "auto_partitioner: Dynamic work stealing, good default" << endl;
}

// ============= Example 2: simple_partitioner =============

void example2_simple_partitioner() {
    cout << "\n=== Example 2: simple_partitioner ===" << endl;
    
    const int N = 10000000;
    vector<int> data(N);
    
    auto start = high_resolution_clock::now();
    
    parallel_for(blocked_range<int>(0, N),
        [&](blocked_range<int> r) {
            for(int i = r.begin(); i != r.end(); ++i) {
                data[i] = i * 2;
            }
        },
        simple_partitioner());  // Equal chunks, no stealing
    
    auto time = duration_cast<milliseconds>(
        high_resolution_clock::now() - start).count();
    
    cout << "Processed " << N << " elements" << endl;
    cout << "Time: " << time << " ms" << endl;
    cout << "simple_partitioner: Minimal overhead, uniform work" << endl;
}

// ============= Example 3: static_partitioner =============

void example3_static_partitioner() {
    cout << "\n=== Example 3: static_partitioner ===" << endl;
    
    const int N = 1000000;
    vector<int> data(N);
    
    cout << "Running same loop twice with static_partitioner:" << endl;
    
    // First run
    parallel_for(blocked_range<int>(0, N),
        [&](blocked_range<int> r) {
            for(int i = r.begin(); i != r.end(); ++i) {
                data[i] = i % 256;
            }
        },
        static_partitioner());
    
    int checksum1 = 0;
    for(int i = 0; i < 100; ++i) checksum1 += data[i];
    
    // Second run
    parallel_for(blocked_range<int>(0, N),
        [&](blocked_range<int> r) {
            for(int i = r.begin(); i != r.end(); ++i) {
                data[i] = i % 256;
            }
        },
        static_partitioner());
    
    int checksum2 = 0;
    for(int i = 0; i < 100; ++i) checksum2 += data[i];
    
    cout << "Checksum 1: " << checksum1 << endl;
    cout << "Checksum 2: " << checksum2 << endl;
    cout << "static_partitioner: Deterministic, reproducible" << endl;
}

// ============= Example 4: affinity_partitioner =============

void example4_affinity_partitioner() {
    cout << "\n=== Example 4: affinity_partitioner ===" << endl;
    
    const int N = 5000000;
    vector<double> data(N, 1.0);
    
    // Create affinity_partitioner (reusable)
    affinity_partitioner ap;
    
    cout << "Running loop 3 times with same affinity_partitioner:" << endl;
    
    for(int iteration = 1; iteration <= 3; ++iteration) {
        auto start = high_resolution_clock::now();
        
        parallel_for(blocked_range<int>(0, N),
            [&](blocked_range<int> r) {
                for(int i = r.begin(); i != r.end(); ++i) {
                    data[i] = sqrt(data[i]) + 1.0;
                }
            },
            ap);  // Reuse same partitioner
        
        auto time = duration_cast<milliseconds>(
            high_resolution_clock::now() - start).count();
        
        cout << "  Iteration " << iteration << ": " << time << " ms" << endl;
    }
    
    cout << "affinity_partitioner: Improves cache reuse across iterations" << endl;
}

// ============= Example 5: Comparing Partitioners =============

void example5_comparison() {
    cout << "\n=== Example 5: Partitioner Comparison ===" << endl;
    
    const int N = 10000000;
    vector<int> data(N);
    
    // Uniform workload
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
    
    // simple_partitioner
    {
        auto start = high_resolution_clock::now();
        parallel_for(blocked_range<int>(0, N), uniform_work, simple_partitioner());
        auto time = duration_cast<milliseconds>(
            high_resolution_clock::now() - start).count();
        cout << "  simple_partitioner: " << time << " ms" << endl;
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

// ============= Example 6: Non-Uniform Workload =============

void example6_nonuniform() {
    cout << "\n=== Example 6: Non-Uniform Workload ===" << endl;
    
    const int N = 100000;
    vector<double> results(N);
    
    // Non-uniform: more work for higher indices
    auto nonuniform_work = [&](blocked_range<int> r) {
        for(int i = r.begin(); i != r.end(); ++i) {
            double sum = 0.0;
            // Variable work: i iterations
            for(int j = 0; j < i % 1000; ++j) {
                sum += sqrt(j + 1.0);
            }
            results[i] = sum;
        }
    };
    
    cout << "Non-uniform workload (variable iterations per element):" << endl;
    
    // auto_partitioner (good for imbalanced work)
    {
        auto start = high_resolution_clock::now();
        parallel_for(blocked_range<int>(0, N), nonuniform_work, auto_partitioner());
        auto time = duration_cast<milliseconds>(
            high_resolution_clock::now() - start).count();
        cout << "  auto_partitioner:   " << time << " ms (adapts to imbalance)" << endl;
    }
    
    // simple_partitioner (poor for imbalanced work)
    {
        auto start = high_resolution_clock::now();
        parallel_for(blocked_range<int>(0, N), nonuniform_work, simple_partitioner());
        auto time = duration_cast<milliseconds>(
            high_resolution_clock::now() - start).count();
        cout << "  simple_partitioner: " << time << " ms (may have imbalance)" << endl;
    }
}

// ============= Example 7: Grain Size Impact =============

void example7_grain_size() {
    cout << "\n=== Example 7: Grain Size Impact ===" << endl;
    
    const int N = 1000000;
    vector<int> data(N);
    
    auto work = [&](blocked_range<int> r) {
        for(int i = r.begin(); i != r.end(); ++i) {
            data[i] = i * i;
        }
    };
    
    cout << "Testing different grain sizes:" << endl;
    
    for(int grain : {100, 1000, 10000, 100000}) {
        auto start = high_resolution_clock::now();
        
        parallel_for(blocked_range<int>(0, N, grain), work, auto_partitioner());
        
        auto time = duration_cast<milliseconds>(
            high_resolution_clock::now() - start).count();
        
        cout << "  Grain size " << grain << ": " << time << " ms" << endl;
    }
    
    cout << "Larger grain = fewer chunks = less overhead" << endl;
    cout << "Smaller grain = more chunks = better load balancing" << endl;
}

int main() {
    cout << "╔════════════════════════════════════════════════════════╗" << endl;
    cout << "║      TBB partitioners - Complete Tutorial             ║" << endl;
    cout << "╚════════════════════════════════════════════════════════╝" << endl;
    
    example1_auto_partitioner();
    example2_simple_partitioner();
    example3_static_partitioner();
    example4_affinity_partitioner();
    example5_comparison();
    example6_nonuniform();
    example7_grain_size();
    
    cout << "\n╔════════════════════════════════════════════════════════╗" << endl;
    cout << "║                    KEY TAKEAWAYS                       ║" << endl;
    cout << "╠════════════════════════════════════════════════════════╣" << endl;
    cout << "║                                                        ║" << endl;
    cout << "║  1. Partitioners control work distribution            ║" << endl;
    cout << "║  2. auto_partitioner: Default, dynamic balancing      ║" << endl;
    cout << "║  3. simple_partitioner: Equal chunks, minimal overhead║" << endl;
    cout << "║  4. static_partitioner: Deterministic, cache-friendly ║" << endl;
    cout << "║  5. affinity_partitioner: Cache reuse across loops    ║" << endl;
    cout << "║  6. Grain size affects overhead vs load balancing     ║" << endl;
    cout << "║  7. Choose based on workload characteristics          ║" << endl;
    cout << "║                                                        ║" << endl;
    cout << "╠════════════════════════════════════════════════════════╣" << endl;
    cout << "║               PARTITIONER SUMMARY                      ║" << endl;
    cout << "╠════════════════════════════════════════════════════════╣" << endl;
    cout << "║                                                        ║" << endl;
    cout << "║  auto_partitioner:                                     ║" << endl;
    cout << "║    + Dynamic load balancing                            ║" << endl;
    cout << "║    + Work stealing                                     ║" << endl;
    cout << "║    + Adapts to workload imbalance                      ║" << endl;
    cout << "║    - Slight overhead                                   ║" << endl;
    cout << "║    Use: Default choice, unknown workload               ║" << endl;
    cout << "║                                                        ║" << endl;
    cout << "║  simple_partitioner:                                   ║" << endl;
    cout << "║    + Minimal overhead                                  ║" << endl;
    cout << "║    + Simple equal division                             ║" << endl;
    cout << "║    - No load balancing                                 ║" << endl;
    cout << "║    - Poor for imbalanced work                          ║" << endl;
    cout << "║    Use: Uniform workload, minimize overhead            ║" << endl;
    cout << "║                                                        ║" << endl;
    cout << "║  static_partitioner:                                   ║" << endl;
    cout << "║    + Deterministic                                     ║" << endl;
    cout << "║    + Cache-friendly                                    ║" << endl;
    cout << "║    + Reproducible results                              ║" << endl;
    cout << "║    - No work stealing                                  ║" << endl;
    cout << "║    Use: Need determinism, cache locality               ║" << endl;
    cout << "║                                                        ║" << endl;
    cout << "║  affinity_partitioner:                                 ║" << endl;
    cout << "║    + Cache affinity across iterations                  ║" << endl;
    cout << "║    + Reuses thread-to-range mapping                    ║" << endl;
    cout << "║    + Improved cache hit rate                           ║" << endl;
    cout << "║    - Must reuse same partitioner object                ║" << endl;
    cout << "║    Use: Repeated loops on same data                    ║" << endl;
    cout << "║                                                        ║" << endl;
    cout << "╠════════════════════════════════════════════════════════╣" << endl;
    cout << "║               USAGE PATTERNS                           ║" << endl;
    cout << "╠════════════════════════════════════════════════════════╣" << endl;
    cout << "║                                                        ║" << endl;
    cout << "║  Default (auto):                                       ║" << endl;
    cout << "║    parallel_for(range, body);                         ║" << endl;
    cout << "║                                                        ║" << endl;
    cout << "║  Explicit:                                             ║" << endl;
    cout << "║    parallel_for(range, body, simple_partitioner());   ║" << endl;
    cout << "║                                                        ║" << endl;
    cout << "║  Reusable affinity:                                    ║" << endl;
    cout << "║    affinity_partitioner ap;                           ║" << endl;
    cout << "║    for(...) {                                          ║" << endl;
    cout << "║        parallel_for(range, body, ap);                 ║" << endl;
    cout << "║    }                                                   ║" << endl;
    cout << "║                                                        ║" << endl;
    cout << "╠════════════════════════════════════════════════════════╣" << endl;
    cout << "║               WHEN TO USE                              ║" << endl;
    cout << "╠════════════════════════════════════════════════════════╣" << endl;
    cout << "║                                                        ║" << endl;
    cout << "║  auto_partitioner:                                     ║" << endl;
    cout << "║    ✓ Default choice                                    ║" << endl;
    cout << "║    ✓ Unknown workload characteristics                 ║" << endl;
    cout << "║    ✓ Potentially imbalanced work                       ║" << endl;
    cout << "║                                                        ║" << endl;
    cout << "║  simple_partitioner:                                   ║" << endl;
    cout << "║    ✓ Uniform workload per element                     ║" << endl;
    cout << "║    ✓ Minimize partitioning overhead                   ║" << endl;
    cout << "║    ✓ Large grain size                                  ║" << endl;
    cout << "║                                                        ║" << endl;
    cout << "║  static_partitioner:                                   ║" << endl;
    cout << "║    ✓ Need deterministic results                        ║" << endl;
    cout << "║    ✓ Debugging/testing                                 ║" << endl;
    cout << "║    ✓ Cache locality important                          ║" << endl;
    cout << "║                                                        ║" << endl;
    cout << "║  affinity_partitioner:                                 ║" << endl;
    cout << "║    ✓ Repeated parallel_for on same data               ║" << endl;
    cout << "║    ✓ Cache reuse across iterations                     ║" << endl;
    cout << "║    ✓ Iterative algorithms                              ║" << endl;
    cout << "║                                                        ║" << endl;
    cout << "╚════════════════════════════════════════════════════════╝" << endl;
    
    return 0;
}
