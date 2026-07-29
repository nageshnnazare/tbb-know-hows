/*
 * TBB GLOBAL_CONTROL - Control TBB Runtime Behavior
 * 
 * DEFINITION:
 * global_control allows you to control various aspects of the TBB runtime,
 * most commonly the maximum number of threads.
 * 
 * CONTROL PARAMETERS:
 * - max_allowed_parallelism: Maximum number of worker threads
 * - thread_stack_size: Stack size for worker threads
 * - terminate_on_exception: Behavior on unhandled exceptions
 * 
 * KEY FEATURES:
 * - RAII-based (effect lasts until object destroyed)
 * - Can be nested (innermost takes precedence)
 * - Thread-safe
 * - Affects all TBB operations in scope
 * 
 * WHEN TO USE:
 * - Limit parallelism for resource constraints
 * - Testing with specific thread counts
 * - Mixed workloads (adjust for each phase)
 * - Container/VM environments
 * - Power/thermal management
 */

#define TBB_PREVIEW_GLOBAL_CONTROL 1

#include <iostream>
#include <vector>
#include <chrono>
#include <thread>
#include <cmath>
#include <tbb/global_control.h>
#include <tbb/parallel_for.h>
#include <tbb/parallel_reduce.h>
#include <tbb/blocked_range.h>
#include <tbb/task_arena.h>

using namespace std;
using namespace tbb;
using namespace chrono;

// ============= Example 1: Basic Thread Control =============

void example1_basic() {
    cout << "\n=== Example 1: Basic Thread Control ===" << endl;
    
    // Default: uses all available cores
    cout << "Hardware concurrency: " << thread::hardware_concurrency() << endl;
    
    const int N = 10000000;
    vector<int> data(N, 1);
    
    // Test with different thread counts
    for(int threads : {1, 2, 4}) {
        global_control gc(global_control::max_allowed_parallelism, threads);
        
        auto start = high_resolution_clock::now();
        
        parallel_for(blocked_range<int>(0, N),
            [&](blocked_range<int> r) {
                for(int i = r.begin(); i != r.end(); ++i) {
                    data[i] *= 2;
                }
            });
        
        auto time = duration_cast<milliseconds>(
            high_resolution_clock::now() - start).count();
        
        cout << threads << " threads: " << time << " ms" << endl;
    }
}

// ============= Example 2: RAII Scoping =============

void example2_scoping() {
    cout << "\n=== Example 2: RAII Scoping ===" << endl;
    
    const int N = 1000000;
    vector<double> data(N, 1.0);
    
    cout << "Default parallelism:" << endl;
    {
        auto start = high_resolution_clock::now();
        parallel_for(blocked_range<int>(0, N),
            [&](blocked_range<int> r) {
                for(int i = r.begin(); i != r.end(); ++i) {
                    data[i] = data[i] * 1.1;
                }
            });
        auto time = duration_cast<microseconds>(
            high_resolution_clock::now() - start).count();
        cout << "  Time: " << time << " μs" << endl;
    }
    
    cout << "Limited to 2 threads:" << endl;
    {
        global_control gc(global_control::max_allowed_parallelism, 2);
        
        auto start = high_resolution_clock::now();
        parallel_for(blocked_range<int>(0, N),
            [&](blocked_range<int> r) {
                for(int i = r.begin(); i != r.end(); ++i) {
                    data[i] = data[i] * 1.1;
                }
            });
        auto time = duration_cast<microseconds>(
            high_resolution_clock::now() - start).count();
        cout << "  Time: " << time << " μs" << endl;
    }  // gc destroyed, parallelism restored
    
    cout << "Back to default parallelism" << endl;
}

// ============= Example 3: Nested Controls =============

void example3_nested() {
    cout << "\n=== Example 3: Nested Controls ===" << endl;
    
    const int N = 5000000;
    vector<int> data(N, 1);
    
    cout << "Outer control: 8 threads" << endl;
    global_control outer(global_control::max_allowed_parallelism, 8);
    
    {
        auto start = high_resolution_clock::now();
        parallel_for(blocked_range<int>(0, N),
            [&](blocked_range<int> r) {
                for(int i = r.begin(); i != r.end(); ++i) {
                    data[i] += 1;
                }
            });
        auto time = duration_cast<milliseconds>(
            high_resolution_clock::now() - start).count();
        cout << "  Time with 8 threads: " << time << " ms" << endl;
    }
    
    {
        cout << "Inner control: 2 threads (overrides outer)" << endl;
        global_control inner(global_control::max_allowed_parallelism, 2);
        
        auto start = high_resolution_clock::now();
        parallel_for(blocked_range<int>(0, N),
            [&](blocked_range<int> r) {
                for(int i = r.begin(); i != r.end(); ++i) {
                    data[i] += 1;
                }
            });
        auto time = duration_cast<milliseconds>(
            high_resolution_clock::now() - start).count();
        cout << "  Time with 2 threads: " << time << " ms" << endl;
    }  // inner destroyed
    
    {
        cout << "Back to outer: 8 threads" << endl;
        auto start = high_resolution_clock::now();
        parallel_for(blocked_range<int>(0, N),
            [&](blocked_range<int> r) {
                for(int i = r.begin(); i != r.end(); ++i) {
                    data[i] += 1;
                }
            });
        auto time = duration_cast<milliseconds>(
            high_resolution_clock::now() - start).count();
        cout << "  Time with 8 threads: " << time << " ms" << endl;
    }
}

// ============= Example 4: Performance Scaling =============

void example4_scaling() {
    cout << "\n=== Example 4: Performance Scaling ===" << endl;
    
    const int N = 20000000;
    
    cout << "Measuring speedup with different thread counts:" << endl;
    
    long long baseline_time = 0;
    
    for(int threads = 1; threads <= thread::hardware_concurrency(); threads *= 2) {
        global_control gc(global_control::max_allowed_parallelism, threads);
        
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
        
        if(threads == 1) baseline_time = time;
        double speedup = (double)baseline_time / time;
        
        cout << threads << " thread(s): " << time << " ms, "
             << "speedup: " << speedup << "x" << endl;
    }
}

// ============= Example 5: Mixed Workload =============

void heavy_computation(vector<double>& data) {
    parallel_for(blocked_range<int>(0, data.size()),
        [&](blocked_range<int> r) {
            for(int i = r.begin(); i != r.end(); ++i) {
                for(int j = 0; j < 100; ++j) {
                    data[i] = sqrt(data[i] + 0.1);
                }
            }
        });
}

void light_computation(vector<int>& data) {
    parallel_for(blocked_range<int>(0, data.size()),
        [&](blocked_range<int> r) {
            for(int i = r.begin(); i != r.end(); ++i) {
                data[i] *= 2;
            }
        });
}

void example5_mixed_workload() {
    cout << "\n=== Example 5: Mixed Workload ===" << endl;
    
    vector<double> heavy_data(10000, 1.0);
    vector<int> light_data(1000000, 1);
    
    cout << "Heavy computation (use all cores):" << endl;
    {
        global_control gc(global_control::max_allowed_parallelism,
            thread::hardware_concurrency());
        
        auto start = high_resolution_clock::now();
        heavy_computation(heavy_data);
        auto time = duration_cast<milliseconds>(
            high_resolution_clock::now() - start).count();
        
        cout << "  Time: " << time << " ms" << endl;
    }
    
    cout << "Light computation (limit threads to avoid overhead):" << endl;
    {
        global_control gc(global_control::max_allowed_parallelism, 4);
        
        auto start = high_resolution_clock::now();
        light_computation(light_data);
        auto time = duration_cast<milliseconds>(
            high_resolution_clock::now() - start).count();
        
        cout << "  Time: " << time << " ms" << endl;
    }
}

// ============= Example 6: Resource-Constrained Environment =============

void example6_resource_constrained() {
    cout << "\n=== Example 6: Resource-Constrained Environment ===" << endl;
    
    cout << "Simulating container with CPU limit:" << endl;
    
    // In a container with CPU limit of 2 cores
    const int CONTAINER_CPUS = 2;
    global_control gc(global_control::max_allowed_parallelism, CONTAINER_CPUS);
    
    const int N = 10000000;
    vector<int> data(N, 1);
    
    auto start = high_resolution_clock::now();
    
    parallel_for(blocked_range<int>(0, N),
        [&](blocked_range<int> r) {
            for(int i = r.begin(); i != r.end(); ++i) {
                data[i] = i % 1000;
            }
        });
    
    auto time = duration_cast<milliseconds>(
        high_resolution_clock::now() - start).count();
    
    cout << "Limited to " << CONTAINER_CPUS << " threads" << endl;
    cout << "Time: " << time << " ms" << endl;
    cout << "Prevents oversubscription in containers/VMs" << endl;
}

// ============= Example 7: Dynamic Thread Adjustment =============

void example7_dynamic() {
    cout << "\n=== Example 7: Dynamic Thread Adjustment ===" << endl;
    
    const int N = 5000000;
    vector<double> data(N, 1.0);
    
    cout << "Adjusting threads based on system load (simulated):" << endl;
    
    // Simulate different load conditions
    for(int load_percent = 25; load_percent <= 100; load_percent += 25) {
        int threads = max(1, (int)(thread::hardware_concurrency() * load_percent / 100));
        
        global_control gc(global_control::max_allowed_parallelism, threads);
        
        auto start = high_resolution_clock::now();
        
        parallel_for(blocked_range<int>(0, N),
            [&](blocked_range<int> r) {
                for(int i = r.begin(); i != r.end(); ++i) {
                    data[i] = sqrt(data[i] + 0.01);
                }
            });
        
        auto time = duration_cast<milliseconds>(
            high_resolution_clock::now() - start).count();
        
        cout << load_percent << "% capacity (" << threads << " threads): "
             << time << " ms" << endl;
    }
}

int main() {
    cout << "╔════════════════════════════════════════════════════════╗" << endl;
    cout << "║     TBB global_control - Complete Tutorial            ║" << endl;
    cout << "╚════════════════════════════════════════════════════════╝" << endl;
    
    example1_basic();
    example2_scoping();
    example3_nested();
    example4_scaling();
    example5_mixed_workload();
    example6_resource_constrained();
    example7_dynamic();
    
    cout << "\n╔════════════════════════════════════════════════════════╗" << endl;
    cout << "║                    KEY TAKEAWAYS                       ║" << endl;
    cout << "╠════════════════════════════════════════════════════════╣" << endl;
    cout << "║                                                        ║" << endl;
    cout << "║  1. Control maximum thread count globally             ║" << endl;
    cout << "║  2. RAII-based (effect lasts until destruction)       ║" << endl;
    cout << "║  3. Nested controls: innermost takes precedence       ║" << endl;
    cout << "║  4. Thread-safe and easy to use                       ║" << endl;
    cout << "║  5. Useful for resource-constrained environments      ║" << endl;
    cout << "║  6. Can adjust dynamically during execution           ║" << endl;
    cout << "║  7. Affects all TBB operations in scope               ║" << endl;
    cout << "║                                                        ║" << endl;
    cout << "╠════════════════════════════════════════════════════════╣" << endl;
    cout << "║               USAGE PATTERNS                           ║" << endl;
    cout << "╠════════════════════════════════════════════════════════╣" << endl;
    cout << "║                                                        ║" << endl;
    cout << "║  Limit Threads:                                        ║" << endl;
    cout << "║    global_control gc(                                 ║" << endl;
    cout << "║        global_control::max_allowed_parallelism, 4);   ║" << endl;
    cout << "║    // All TBB operations limited to 4 threads         ║" << endl;
    cout << "║                                                        ║" << endl;
    cout << "║  Scoped Control:                                       ║" << endl;
    cout << "║    {                                                   ║" << endl;
    cout << "║        global_control gc(...);                        ║" << endl;
    cout << "║        // Limited parallelism                          ║" << endl;
    cout << "║    }  // Restored when gc destroyed                    ║" << endl;
    cout << "║                                                        ║" << endl;
    cout << "║  Nested:                                               ║" << endl;
    cout << "║    global_control outer(..., 8);                      ║" << endl;
    cout << "║    {                                                   ║" << endl;
    cout << "║        global_control inner(..., 2);  // Overrides    ║" << endl;
    cout << "║    }  // Back to 8                                     ║" << endl;
    cout << "║                                                        ║" << endl;
    cout << "╠════════════════════════════════════════════════════════╣" << endl;
    cout << "║               CONTROL PARAMETERS                       ║" << endl;
    cout << "╠════════════════════════════════════════════════════════╣" << endl;
    cout << "║                                                        ║" << endl;
    cout << "║  max_allowed_parallelism:                              ║" << endl;
    cout << "║    Maximum number of worker threads                    ║" << endl;
    cout << "║    Default: hardware_concurrency()                     ║" << endl;
    cout << "║    Use 1 for sequential execution                      ║" << endl;
    cout << "║                                                        ║" << endl;
    cout << "║  thread_stack_size:                                    ║" << endl;
    cout << "║    Stack size for TBB worker threads                   ║" << endl;
    cout << "║    For deep recursion                                  ║" << endl;
    cout << "║                                                        ║" << endl;
    cout << "║  terminate_on_exception:                               ║" << endl;
    cout << "║    Behavior on unhandled exceptions                    ║" << endl;
    cout << "║                                                        ║" << endl;
    cout << "╠════════════════════════════════════════════════════════╣" << endl;
    cout << "║               WHEN TO USE                              ║" << endl;
    cout << "╠════════════════════════════════════════════════════════╣" << endl;
    cout << "║                                                        ║" << endl;
    cout << "║  ✓ Running in containers/VMs with CPU limits          ║" << endl;
    cout << "║  ✓ Testing with specific thread counts               ║" << endl;
    cout << "║  ✓ Mixed workloads (adjust per phase)                ║" << endl;
    cout << "║  ✓ Power/thermal management                           ║" << endl;
    cout << "║  ✓ Avoiding oversubscription                          ║" << endl;
    cout << "║  ✓ Benchmarking scalability                           ║" << endl;
    cout << "║  ✓ Resource quotas                                    ║" << endl;
    cout << "║  ✗ Default settings usually optimal                   ║" << endl;
    cout << "║                                                        ║" << endl;
    cout << "╚════════════════════════════════════════════════════════╝" << endl;
    
    return 0;
}
