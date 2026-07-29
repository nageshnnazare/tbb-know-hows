/*
 * TBB TASK_SCHEDULER_INIT - Scheduler Initialization and Control
 * 
 * DEFINITION:
 * task_scheduler_init initializes the TBB task scheduler and controls
 * the number of threads used by TBB.
 * 
 * KEY FEATURES:
 * - Control thread count
 * - Initialize/terminate scheduler
 * - Query default threads
 * - Multiple init objects supported
 * - RAII-based lifetime management
 * 
 * WHEN TO USE:
 * - Need to limit thread count
 * - Initialize scheduler early
 * - Control scheduler lifetime
 * - Debug/test with specific threads
 * 
 * IMPORTANT:
 * - Usually not needed (TBB auto-initializes)
 * - Use task_arena for most cases
 * - Affects all TBB operations
 */

#define TBB_PREVIEW_GLOBAL_CONTROL 1

#include <iostream>
#include <vector>
#include <chrono>
#include <cmath>
#include <tbb/task_scheduler_init.h>
#include <tbb/parallel_for.h>
#include <tbb/blocked_range.h>
#include <tbb/global_control.h> 

using namespace std;
using namespace tbb;
using namespace chrono;

// ============= Example 1: Default vs Custom Thread Count =============

void example1_thread_control() {
    cout << "\n=== Example 1: Thread Count Control ===" << endl;
    
    cout << "System hardware threads: " 
         << task_scheduler_init::default_num_threads() << endl;
    
    // Default initialization (automatic)
    {
        task_scheduler_init init;
        cout << "Default init - using all available threads" << endl;
        
        tbb::atomic<int> sum;
        sum = 0;
        parallel_for(blocked_range<int>(0, 1000),
            [&](blocked_range<int> r) {
                for(int i = r.begin(); i != r.end(); ++i)
                    sum += i;
            });
        
        cout << "Sum: " << sum << " (computed with default threads)" << endl;
    }
    
    // Custom thread count
    {
        task_scheduler_init init(4);  // Use exactly 4 threads
        cout << "\nCustom init - using 4 threads" << endl;
        
        tbb::atomic<int> sum;
        sum = 0;
        parallel_for(blocked_range<int>(0, 1000),
            [&](blocked_range<int> r) {
                for(int i = r.begin(); i != r.end(); ++i)
                    sum += i;
            });
        
        cout << "Sum: " << sum << " (computed with 4 threads)" << endl;
    }
}

// ============= Example 2: Performance with Different Thread Counts =============

void example2_performance() {
    cout << "\n=== Example 2: Performance vs Thread Count ===" << endl;
    
    const int N = 10000000;
    vector<double> data(N);
    for(int i = 0; i < N; ++i) data[i] = i;
    
    // Test with different thread counts
    vector<int> thread_counts = {1, 2, 4, 8};
    
    for(int threads : thread_counts) {
        if(threads > task_scheduler_init::default_num_threads())
            continue;
        
        task_scheduler_init init(threads);
        
        auto start = high_resolution_clock::now();
        
        double sum = 0;
        parallel_for(blocked_range<int>(0, N),
            [&](blocked_range<int> r) {
                double local_sum = 0;
                for(int i = r.begin(); i != r.end(); ++i)
                    local_sum += data[i];
                sum += local_sum;
            });
        
        auto elapsed = duration_cast<milliseconds>(
            high_resolution_clock::now() - start).count();
        
        cout << threads << " threads: " << elapsed << " ms" << endl;
    }
}

// ============= Example 3: Automatic vs Manual Initialization =============

void example3_automatic() {
    cout << "\n=== Example 3: Automatic Initialization ===" << endl;
    
    // TBB automatically initializes on first use
    cout << "No explicit init - TBB auto-initializes" << endl;
    
    parallel_for(blocked_range<int>(0, 100),
        [](blocked_range<int> r) {
            for(int i = r.begin(); i != r.end(); ++i) {
                // Work
            }
        });
    
    cout << "Parallel work completed (auto-init happened)" << endl;
    cout << "Hardware concurrency: " 
         << task_scheduler_init::default_num_threads() << endl;
}

// ============= Example 4: Nested Initialization =============

void example4_nested() {
    cout << "\n=== Example 4: Nested Initialization ===" << endl;
    
    task_scheduler_init outer_init(4);
    cout << "Outer scope: 4 threads" << endl;
    
    {
        // Inner scope can have different settings
        task_scheduler_init inner_init(2);
        cout << "Inner scope: 2 threads" << endl;
        
        parallel_for(blocked_range<int>(0, 100),
            [](blocked_range<int> r) {
                // Uses 2 threads
            });
        
        cout << "Inner scope work done" << endl;
    } // inner_init destroyed
    
    parallel_for(blocked_range<int>(0, 100),
        [](blocked_range<int> r) {
            // Back to 4 threads
        });
    
    cout << "Outer scope work done" << endl;
}

// ============= Example 5: Terminate and Reinitialize =============

void example5_terminate() {
    cout << "\n=== Example 5: Terminate and Reinitialize ===" << endl;
    
    {
        task_scheduler_init init(4);
        cout << "Scheduler initialized with 4 threads" << endl;
        
        parallel_for(blocked_range<int>(0, 1000),
            [](blocked_range<int> r) {
                for(int i = r.begin(); i != r.end(); ++i) {
                    double x = sin(i * 0.001);
                }
            });
        
        cout << "Work completed" << endl;
    } // Scheduler terminated
    
    cout << "Scheduler terminated" << endl;
    
    {
        task_scheduler_init init(2);
        cout << "Scheduler reinitialized with 2 threads" << endl;
        
        parallel_for(blocked_range<int>(0, 1000),
            [](blocked_range<int> r) {
                for(int i = r.begin(); i != r.end(); ++i) {
                    double x = cos(i * 0.001);
                }
            });
        
        cout << "Work completed with new settings" << endl;
    }
}

// ============= Example 6: Global Control (Modern Alternative) =============

void example6_global_control() {
    cout << "\n=== Example 6: global_control (Modern API) ===" << endl;
    
    // Modern way to control thread count (TBB 2018+)
    global_control gc(global_control::max_allowed_parallelism, 4);
    
    cout << "Using global_control to limit to 4 threads" << endl;
    
    parallel_for(blocked_range<int>(0, 1000),
        [](blocked_range<int> r) {
            for(int i = r.begin(); i != r.end(); ++i) {
                double x = sin(i * 0.001) * cos(i * 0.001);
            }
        });
    
    cout << "Work completed with global_control" << endl;
    cout << "Note: global_control is preferred over task_scheduler_init" << endl;
}

// ============= Example 7: Testing and Debugging =============

void example7_testing() {
    cout << "\n=== Example 7: Testing with Single Thread ===" << endl;
    
    // Useful for testing/debugging
    task_scheduler_init init(1);  // Sequential execution
    
    cout << "Running with 1 thread (sequential)" << endl;
    cout << "Useful for:" << endl;
    cout << "  - Debugging race conditions" << endl;
    cout << "  - Deterministic testing" << endl;
    cout << "  - Performance baseline" << endl;
    
    vector<int> data(1000);
    
    auto start = high_resolution_clock::now();
    parallel_for(blocked_range<int>(0, data.size()),
        [&](blocked_range<int> r) {
            for(int i = r.begin(); i != r.end(); ++i)
                data[i] = i * i;
        });
    auto elapsed = duration_cast<microseconds>(
        high_resolution_clock::now() - start).count();
    
    cout << "Sequential time: " << elapsed << " μs" << endl;
    cout << "No race conditions possible with 1 thread" << endl;
}

int main() {
    cout << "╔════════════════════════════════════════════════════════╗" << endl;
    cout << "║   TBB task_scheduler_init - Complete Tutorial         ║" << endl;
    cout << "╚════════════════════════════════════════════════════════╝" << endl;
    
    example1_thread_control();
    example2_performance();
    example3_automatic();
    example4_nested();
    example5_terminate();
    example6_global_control();
    example7_testing();
    
    cout << "\n╔════════════════════════════════════════════════════════╗" << endl;
    cout << "║                    KEY TAKEAWAYS                       ║" << endl;
    cout << "╠════════════════════════════════════════════════════════╣" << endl;
    cout << "║                                                        ║" << endl;
    cout << "║  1. Usually NOT needed - TBB auto-initializes          ║" << endl;
    cout << "║  2. Use to control thread count explicitly            ║" << endl;
    cout << "║  3. RAII-based - destroyed when out of scope          ║" << endl;
    cout << "║  4. default_num_threads() returns hardware threads    ║" << endl;
    cout << "║  5. Affects ALL TBB operations in scope               ║" << endl;
    cout << "║  6. global_control is modern alternative (TBB 2018+)  ║" << endl;
    cout << "║  7. Use task_arena for finer-grained control          ║" << endl;
    cout << "║                                                        ║" << endl;
    cout << "╠════════════════════════════════════════════════════════╣" << endl;
    cout << "║               COMMON PATTERNS                          ║" << endl;
    cout << "╠════════════════════════════════════════════════════════╣" << endl;
    cout << "║                                                        ║" << endl;
    cout << "║  Limit Threads:                                        ║" << endl;
    cout << "║    task_scheduler_init init(4);  // Use 4 threads     ║" << endl;
    cout << "║    // All TBB work uses 4 threads                     ║" << endl;
    cout << "║                                                        ║" << endl;
    cout << "║  Modern API:                                           ║" << endl;
    cout << "║    global_control gc(                                  ║" << endl;
    cout << "║      global_control::max_allowed_parallelism, 4);     ║" << endl;
    cout << "║                                                        ║" << endl;
    cout << "║  Query Hardware:                                       ║" << endl;
    cout << "║    int n = task_scheduler_init::default_num_threads();║" << endl;
    cout << "║                                                        ║" << endl;
    cout << "╠════════════════════════════════════════════════════════╣" << endl;
    cout << "║               WHEN TO USE                              ║" << endl;
    cout << "╠════════════════════════════════════════════════════════╣" << endl;
    cout << "║                                                        ║" << endl;
    cout << "║  ✓ Need to limit overall thread count                 ║" << endl;
    cout << "║  ✓ Testing with single thread                         ║" << endl;
    cout << "║  ✓ Resource-constrained environments                  ║" << endl;
    cout << "║  ✓ Debugging race conditions                          ║" << endl;
    cout << "║  ✗ Fine-grained control (use task_arena instead)      ║" << endl;
    cout << "║  ✗ Most applications (auto-init is fine)              ║" << endl;
    cout << "║                                                        ║" << endl;
    cout << "╚════════════════════════════════════════════════════════╝" << endl;
    
    return 0;
}
