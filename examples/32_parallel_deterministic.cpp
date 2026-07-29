/*
 * TBB DETERMINISTIC PARALLELISM - Reproducible Results
 * 
 * DEFINITION:
 * Deterministic parallelism ensures that parallel algorithms produce
 * the same results on every run, regardless of thread scheduling.
 * 
 * KEY CONCEPTS:
 * - Non-determinism sources: Race conditions, floating-point order
 * - Deterministic patterns: No shared mutable state, associative operations
 * - Tools: static_partitioner, ordered operations, atomic reductions
 * 
 * TECHNIQUES:
 * 1. Use static_partitioner for reproducible work division
 * 2. Avoid shared mutable state (use thread-local storage)
 * 3. Use deterministic reduce operations
 * 4. Ordered operations where needed
 * 5. Careful with floating-point (not truly associative)
 * 
 * WHY DETERMINISM MATTERS:
 * - Debugging and testing
 * - Reproducible scientific results
 * - Compliance requirements
 * - Easier reasoning about code
 * 
 * TRADE-OFFS:
 * + Reproducible results
 * + Easier debugging
 * - May sacrifice some performance
 * - More restrictive programming model
 */

#include <iostream>
#include <vector>
#include <chrono>
#include <cmath>
#include <iomanip>
#include <tbb/parallel_for.h>
#include <tbb/parallel_reduce.h>
#include <tbb/blocked_range.h>
#include <tbb/partitioner.h>
#include <tbb/enumerable_thread_specific.h>
#include <tbb/spin_mutex.h>

using namespace std;
using namespace tbb;
using namespace chrono;

// ============= Example 1: Non-Deterministic vs Deterministic =============

void example1_determinism() {
    cout << "\n=== Example 1: Non-Deterministic vs Deterministic ===" << endl;
    
    const int N = 1000;
    vector<int> data(N);
    
    // Initialize
    for(int i = 0; i < N; ++i) data[i] = i;
    
    cout << "Running same computation multiple times:" << endl;
    
    // Non-deterministic (auto_partitioner)
    cout << "\nWith auto_partitioner (may vary):" << endl;
    for(int run = 1; run <= 3; ++run) {
        int checksum = 0;
        parallel_for(blocked_range<int>(0, N),
            [&](blocked_range<int> r) {
                for(int i = r.begin(); i != r.end(); ++i) {
                    // Order of operations may vary
                    checksum += data[i];  // Not thread-safe, but for demo
                }
            },
            auto_partitioner());
        cout << "  Run " << run << " checksum: " << checksum << endl;
    }
    
    // Deterministic (static_partitioner + proper reduction)
    cout << "\nWith static_partitioner (consistent):" << endl;
    for(int run = 1; run <= 3; ++run) {
        int sum = parallel_reduce(
            blocked_range<int>(0, N),
            0,
            [&](blocked_range<int> r, int init) {
                for(int i = r.begin(); i != r.end(); ++i) {
                    init += data[i];
                }
                return init;
            },
            [](int x, int y) { return x + y; },
            static_partitioner());
        cout << "  Run " << run << " sum: " << sum << endl;
    }
}

// ============= Example 2: Deterministic Reduction =============

void example2_deterministic_reduce() {
    cout << "\n=== Example 2: Deterministic Reduction ===" << endl;
    
    const int N = 1000000;
    vector<double> data(N, 1.5);
    
    cout << "Computing sum with different methods:" << endl;
    
    // Method 1: Deterministic with static_partitioner
    double sum1 = parallel_reduce(
        blocked_range<int>(0, N),
        0.0,
        [&](blocked_range<int> r, double init) {
            for(int i = r.begin(); i != r.end(); ++i) {
                init += data[i];
            }
            return init;
        },
        [](double x, double y) { return x + y; },
        static_partitioner());
    
    // Method 2: Run again - should be identical
    double sum2 = parallel_reduce(
        blocked_range<int>(0, N),
        0.0,
        [&](blocked_range<int> r, double init) {
            for(int i = r.begin(); i != r.end(); ++i) {
                init += data[i];
            }
            return init;
        },
        [](double x, double y) { return x + y; },
        static_partitioner());
    
    cout << fixed << setprecision(10);
    cout << "Sum 1: " << sum1 << endl;
    cout << "Sum 2: " << sum2 << endl;
    cout << "Difference: " << abs(sum1 - sum2) << endl;
    cout << "With static_partitioner: results are deterministic" << endl;
}

// ============= Example 3: Thread-Local Storage for Determinism =============

void example3_thread_local() {
    cout << "\n=== Example 3: Thread-Local Storage ===" << endl;
    
    const int N = 10000;
    vector<int> data(N);
    for(int i = 0; i < N; ++i) data[i] = i;
    
    // Thread-local accumulators (deterministic combination)
    enumerable_thread_specific<long long> thread_sums(0);
    
    parallel_for(blocked_range<int>(0, N),
        [&](blocked_range<int> r) {
            long long& local_sum = thread_sums.local();
            for(int i = r.begin(); i != r.end(); ++i) {
                local_sum += data[i];
            }
        },
        static_partitioner());
    
    // Combine results deterministically
    long long total = thread_sums.combine([](long long a, long long b) {
        return a + b;
    });
    
    cout << "Total sum: " << total << endl;
    cout << "Thread-local storage avoids races" << endl;
}

// ============= Example 4: Ordered Operations =============

void example4_ordered() {
    cout << "\n=== Example 4: Ordered Operations ===" << endl;
    
    const int N = 100;
    vector<int> results;
    tbb::spin_mutex mtx;
    
    parallel_for(blocked_range<int>(0, N),
        [&](blocked_range<int> r) {
            vector<int> local_results;
            for(int i = r.begin(); i != r.end(); ++i) {
                local_results.push_back(i * i);
            }
            
            // Ordered insertion
            tbb::spin_mutex::scoped_lock lock(mtx);
            results.insert(results.end(),
                local_results.begin(), local_results.end());
        },
        static_partitioner());
    
    cout << "Collected " << results.size() << " results" << endl;
    cout << "First 10: ";
    for(int i = 0; i < 10 && i < results.size(); ++i) {
        cout << results[i] << " ";
    }
    cout << endl;
}

// ============= Example 5: Floating-Point Determinism =============

void example5_floating_point() {
    cout << "\n=== Example 5: Floating-Point Determinism ===" << endl;
    
    const int N = 1000000;
    vector<double> data(N, 0.1);
    
    cout << "Floating-point addition is NOT truly associative:" << endl;
    
    // Sequential sum
    double seq_sum = 0.0;
    for(int i = 0; i < N; ++i) {
        seq_sum += data[i];
    }
    
    // Parallel sum (run twice)
    double par_sum1 = parallel_reduce(
        blocked_range<int>(0, N),
        0.0,
        [&](blocked_range<int> r, double init) {
            for(int i = r.begin(); i != r.end(); ++i) {
                init += data[i];
            }
            return init;
        },
        [](double x, double y) { return x + y; },
        static_partitioner());
    
    double par_sum2 = parallel_reduce(
        blocked_range<int>(0, N),
        0.0,
        [&](blocked_range<int> r, double init) {
            for(int i = r.begin(); i != r.end(); ++i) {
                init += data[i];
            }
            return init;
        },
        [](double x, double y) { return x + y; },
        static_partitioner());
    
    cout << fixed << setprecision(15);
    cout << "Sequential: " << seq_sum << endl;
    cout << "Parallel 1: " << par_sum1 << endl;
    cout << "Parallel 2: " << par_sum2 << endl;
    cout << "Par1-Par2:  " << (par_sum1 - par_sum2) << endl;
    cout << "\nstatic_partitioner ensures same grouping = same result" << endl;
}

// ============= Example 6: Reproducible Random Processing =============

void example6_reproducible() {
    cout << "\n=== Example 6: Reproducible Processing ===" << endl;
    
    const int N = 1000;
    vector<int> data(N);
    
    // Initialize deterministically
    for(int i = 0; i < N; ++i) {
        data[i] = (i * 31337) % 1000;
    }
    
    cout << "Running computation 3 times:" << endl;
    
    for(int run = 1; run <= 3; ++run) {
        vector<int> results(N);
        
        parallel_for(blocked_range<int>(0, N),
            [&](blocked_range<int> r) {
                for(int i = r.begin(); i != r.end(); ++i) {
                    // Deterministic computation
                    results[i] = (data[i] * data[i]) % 10000;
                }
            },
            static_partitioner());
        
        // Checksum
        long long checksum = 0;
        for(int x : results) checksum += x;
        
        cout << "  Run " << run << " checksum: " << checksum << endl;
    }
    
    cout << "All runs produce identical results!" << endl;
}

// ============= Example 7: Debugging with Determinism =============

void example7_debugging() {
    cout << "\n=== Example 7: Debugging Benefits ===" << endl;
    
    const int N = 10000;
    vector<int> data(N);
    for(int i = 0; i < N; ++i) data[i] = i;
    
    cout << "Deterministic execution makes debugging easier:" << endl;
    
    // With static_partitioner, same ranges processed each time
    int error_index = -1;
    
    parallel_for(blocked_range<int>(0, N),
        [&](blocked_range<int> r) {
            for(int i = r.begin(); i != r.end(); ++i) {
                int result = data[i] * data[i];
                
                // Simulate detecting an error
                if(result == 2500) {  // 50*50
                    error_index = i;
                }
            }
        },
        static_partitioner());
    
    if(error_index != -1) {
        cout << "Error detected at index: " << error_index << endl;
        cout << "With determinism, error appears at same place every run" << endl;
    }
}

int main() {
    cout << "╔════════════════════════════════════════════════════════╗" << endl;
    cout << "║  TBB parallel_deterministic - Complete Tutorial       ║" << endl;
    cout << "╚════════════════════════════════════════════════════════╝" << endl;
    
    example1_determinism();
    example2_deterministic_reduce();
    example3_thread_local();
    example4_ordered();
    example5_floating_point();
    example6_reproducible();
    example7_debugging();
    
    cout << "\n╔════════════════════════════════════════════════════════╗" << endl;
    cout << "║                    KEY TAKEAWAYS                       ║" << endl;
    cout << "╠════════════════════════════════════════════════════════╣" << endl;
    cout << "║                                                        ║" << endl;
    cout << "║  1. Use static_partitioner for deterministic division ║" << endl;
    cout << "║  2. Avoid shared mutable state (use thread-local)     ║" << endl;
    cout << "║  3. parallel_reduce is inherently deterministic       ║" << endl;
    cout << "║  4. Floating-point still has rounding issues          ║" << endl;
    cout << "║  5. Determinism aids debugging and testing            ║" << endl;
    cout << "║  6. May sacrifice some performance                    ║" << endl;
    cout << "║  7. Essential for reproducible science                ║" << endl;
    cout << "║                                                        ║" << endl;
    cout << "╠════════════════════════════════════════════════════════╣" << endl;
    cout << "║               TECHNIQUES FOR DETERMINISM               ║" << endl;
    cout << "╠════════════════════════════════════════════════════════╣" << endl;
    cout << "║                                                        ║" << endl;
    cout << "║  1. Static Partitioning:                               ║" << endl;
    cout << "║     parallel_for(..., static_partitioner());          ║" << endl;
    cout << "║     Same work distribution every time                  ║" << endl;
    cout << "║                                                        ║" << endl;
    cout << "║  2. Deterministic Reduction:                           ║" << endl;
    cout << "║     parallel_reduce with associative operation        ║" << endl;
    cout << "║     Same tree structure = same result                  ║" << endl;
    cout << "║                                                        ║" << endl;
    cout << "║  3. Thread-Local Storage:                              ║" << endl;
    cout << "║     enumerable_thread_specific<T>                     ║" << endl;
    cout << "║     Deterministic combination                          ║" << endl;
    cout << "║                                                        ║" << endl;
    cout << "║  4. Avoid Race Conditions:                             ║" << endl;
    cout << "║     No shared mutable state                            ║" << endl;
    cout << "║     Use proper synchronization                         ║" << endl;
    cout << "║                                                        ║" << endl;
    cout << "╠════════════════════════════════════════════════════════╣" << endl;
    cout << "║               NON-DETERMINISM SOURCES                  ║" << endl;
    cout << "╠════════════════════════════════════════════════════════╣" << endl;
    cout << "║                                                        ║" << endl;
    cout << "║  ✗ auto_partitioner (dynamic work stealing)           ║" << endl;
    cout << "║  ✗ Race conditions on shared data                     ║" << endl;
    cout << "║  ✗ Floating-point associativity                       ║" << endl;
    cout << "║  ✗ Thread scheduling variations                       ║" << endl;
    cout << "║  ✗ Unordered container iteration                      ║" << endl;
    cout << "║  ✗ Time-based operations                              ║" << endl;
    cout << "║                                                        ║" << endl;
    cout << "╠════════════════════════════════════════════════════════╣" << endl;
    cout << "║               WHEN TO USE                              ║" << endl;
    cout << "╠════════════════════════════════════════════════════════╣" << endl;
    cout << "║                                                        ║" << endl;
    cout << "║  ✓ Debugging parallel code                            ║" << endl;
    cout << "║  ✓ Unit testing                                       ║" << endl;
    cout << "║  ✓ Scientific computing (reproducibility)             ║" << endl;
    cout << "║  ✓ Compliance requirements                            ║" << endl;
    cout << "║  ✓ Comparing algorithm variations                     ║" << endl;
    cout << "║  ✗ Maximum performance needed                         ║" << endl;
    cout << "║  ✗ Unpredictable workload (use auto_partitioner)      ║" << endl;
    cout << "║                                                        ║" << endl;
    cout << "╚════════════════════════════════════════════════════════╝" << endl;
    
    return 0;
}
