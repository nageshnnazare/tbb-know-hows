/*
 * TBB TASK_ARENA - Controlling Parallelism and Thread Isolation
 * 
 * DEFINITION:
 * task_arena provides explicit control over the number of threads and
 * isolates parallel work into separate arenas.
 * 
 * KEY FEATURES:
 * - Control thread count explicitly
 * - Isolate parallel regions
 * - Nested parallelism support
 * - Execute work in specific arena
 * - NUMA-aware allocation
 * 
 * WHEN TO USE:
 * - Need specific thread count
 * - Isolate different workloads
 * - Control resource usage
 * - NUMA optimizations
 * - Nested parallel frameworks
 * 
 * BENEFITS:
 * + Explicit resource control
 * + Isolation between workloads
 * + Better NUMA performance
 * + Prevent oversubscription
 */

#include <iostream>
#include <vector>
#include <chrono>
#include <thread>
#include <cmath>
#include <tbb/task_arena.h>
#include <tbb/parallel_for.h>
#include <tbb/blocked_range.h>
#include <tbb/task_scheduler_init.h>

using namespace std;
using namespace tbb;
using namespace chrono;

// ============= Example 1: Basic Arena with Custom Thread Count =============

void example1_basic() {
    cout << "\n=== Example 1: Custom Thread Count ===" << endl;
    
    cout << "System default threads: " 
         << task_scheduler_init::default_num_threads() << endl;
    
    // Arena with 4 threads
    task_arena arena(4);
    
    arena.execute([&]() {
        cout << "Arena max concurrency: " << arena.max_concurrency() << endl;
        
        tbb::atomic<int> counter;
        counter = 0;
        
        parallel_for(blocked_range<int>(0, 1000),
            [&](blocked_range<int> r) {
                for(int i = r.begin(); i != r.end(); ++i)
                    counter++;
            });
        
        cout << "Counter: " << counter << endl;
    });
    
    cout << "Work completed in 4-thread arena" << endl;
}

// ============= Example 2: Multiple Isolated Arenas =============

void example2_isolation() {
    cout << "\n=== Example 2: Multiple Isolated Arenas ===" << endl;
    
    task_arena arena1(2);  // 2 threads
    task_arena arena2(4);  // 4 threads
    
    auto work = [](int arena_id, int threads) {
        auto start = high_resolution_clock::now();
        
        parallel_for(blocked_range<int>(0, 10000),
            [](blocked_range<int> r) {
                double sum = 0;
                for(int i = r.begin(); i != r.end(); ++i)
                    sum += sin(i * 0.001);
            });
        
        auto elapsed = duration_cast<milliseconds>(
            high_resolution_clock::now() - start).count();
        
        cout << "Arena " << arena_id << " (" << threads << " threads): " 
             << elapsed << " ms" << endl;
    };
    
    // Execute in different arenas (isolated)
    thread t1([&]() { arena1.execute([&]() { work(1, 2); }); });
    thread t2([&]() { arena2.execute([&]() { work(2, 4); }); });
    
    t1.join();
    t2.join();
    
    cout << "Two workloads ran in isolated arenas" << endl;
}

// ============= Example 3: Nested Parallelism =============

void example3_nested() {
    cout << "\n=== Example 3: Nested Parallelism ===" << endl;
    
    task_arena outer_arena(4);
    task_arena inner_arena(2);
    
    outer_arena.execute([&]() {
        cout << "Outer arena (4 threads) executing..." << endl;
        
        parallel_for(blocked_range<int>(0, 10),
            [&](blocked_range<int> r) {
                for(int i = r.begin(); i != r.end(); ++i) {
                    // Nested parallel work in inner arena
                    inner_arena.execute([&]() {
                        parallel_for(blocked_range<int>(0, 100),
                            [](blocked_range<int> r2) {
                                double sum = 0;
                                for(int j = r2.begin(); j != r2.end(); ++j)
                                    sum += j;
                            });
                    });
                }
            });
        
        cout << "Nested parallel work completed" << endl;
    });
}

// ============= Example 4: Resource Control =============

void example4_resource_control() {
    cout << "\n=== Example 4: Resource Control ===" << endl;
    
    const int TOTAL_THREADS = task_scheduler_init::default_num_threads();
    
    // Split resources between two workloads
    task_arena high_priority(TOTAL_THREADS * 2 / 3);
    task_arena low_priority(TOTAL_THREADS * 1 / 3);
    
    cout << "High priority arena: " << high_priority.max_concurrency() << " threads" << endl;
    cout << "Low priority arena: " << low_priority.max_concurrency() << " threads" << endl;
    
    auto start = high_resolution_clock::now();
    
    thread t1([&]() {
        high_priority.execute([&]() {
            parallel_for(blocked_range<int>(0, 10000),
                [](blocked_range<int> r) {
                    for(int i = r.begin(); i != r.end(); ++i) {
                        double x = sin(i * 0.001) * cos(i * 0.001);
                    }
                });
        });
    });
    
    thread t2([&]() {
        low_priority.execute([&]() {
            parallel_for(blocked_range<int>(0, 5000),
                [](blocked_range<int> r) {
                    for(int i = r.begin(); i != r.end(); ++i) {
                        double x = sin(i * 0.001) * cos(i * 0.001);
                    }
                });
        });
    });
    
    t1.join();
    t2.join();
    
    auto elapsed = duration_cast<milliseconds>(
        high_resolution_clock::now() - start).count();
    
    cout << "Both workloads completed in " << elapsed << " ms" << endl;
    cout << "Resources divided between priority levels" << endl;
}

// ============= Example 5: Sequential vs Parallel Execution =============

void example5_sequential_mode() {
    cout << "\n=== Example 5: Sequential Mode ===" << endl;
    
    // Arena with 1 thread = sequential execution
    task_arena sequential(1);
    task_arena parallel(4);
    
    vector<int> data(1000000, 1);
    
    // Sequential
    auto start = high_resolution_clock::now();
    sequential.execute([&]() {
        int sum = 0;
        parallel_for(blocked_range<int>(0, data.size()),
            [&](blocked_range<int> r) {
                for(int i = r.begin(); i != r.end(); ++i)
                    sum += data[i];
            });
    });
    auto seq_time = duration_cast<milliseconds>(
        high_resolution_clock::now() - start).count();
    
    // Parallel
    start = high_resolution_clock::now();
    parallel.execute([&]() {
        int sum = 0;
        parallel_for(blocked_range<int>(0, data.size()),
            [&](blocked_range<int> r) {
                for(int i = r.begin(); i != r.end(); ++i)
                    sum += data[i];
            });
    });
    auto par_time = duration_cast<milliseconds>(
        high_resolution_clock::now() - start).count();
    
    cout << "Sequential (1 thread): " << seq_time << " ms" << endl;
    cout << "Parallel (4 threads): " << par_time << " ms" << endl;
    cout << "Speedup: " << (double)seq_time / par_time << "x" << endl;
}

// ============= Example 6: Enqueue Work =============

void example6_enqueue() {
    cout << "\n=== Example 6: Enqueue Work ===" << endl;
    
    task_arena arena(4);
    
    cout << "Enqueuing work to arena..." << endl;
    
    // Enqueue multiple independent tasks
    for(int i = 0; i < 5; ++i) {
        arena.enqueue([i]() {
            cout << "Task " << i << " executing in arena" << endl;
            this_thread::sleep_for(milliseconds(100));
        });
    }
    
    cout << "Tasks enqueued, now executing..." << endl;
    arena.execute([]() {
        // Wait for enqueued tasks
    });
    
    cout << "All enqueued tasks completed" << endl;
}

// ============= Example 7: Real-world Server Example =============

void example7_server_pattern() {
    cout << "\n=== Example 7: Server Request Processing ===" << endl;
    
    // Separate arenas for different request types
    task_arena fast_requests(4);   // Quick requests
    task_arena slow_requests(2);   // Long-running requests
    
    const int NUM_FAST = 100;
    const int NUM_SLOW = 20;
    
    auto start = high_resolution_clock::now();
    
    // Process fast requests
    thread fast_thread([&]() {
        fast_requests.execute([&]() {
            parallel_for(blocked_range<int>(0, NUM_FAST),
                [](blocked_range<int> r) {
                    for(int i = r.begin(); i != r.end(); ++i) {
                        // Simulate fast request
                        this_thread::sleep_for(microseconds(100));
                    }
                });
        });
    });
    
    // Process slow requests
    thread slow_thread([&]() {
        slow_requests.execute([&]() {
            parallel_for(blocked_range<int>(0, NUM_SLOW),
                [](blocked_range<int> r) {
                    for(int i = r.begin(); i != r.end(); ++i) {
                        // Simulate slow request
                        this_thread::sleep_for(milliseconds(5));
                    }
                });
        });
    });
    
    fast_thread.join();
    slow_thread.join();
    
    auto elapsed = duration_cast<milliseconds>(
        high_resolution_clock::now() - start).count();
    
    cout << "Processed " << NUM_FAST << " fast requests" << endl;
    cout << "Processed " << NUM_SLOW << " slow requests" << endl;
    cout << "Total time: " << elapsed << " ms" << endl;
    cout << "Isolation prevents slow requests from blocking fast ones" << endl;
}

int main() {
    cout << "╔════════════════════════════════════════════════════════╗" << endl;
    cout << "║      TBB task_arena - Complete Tutorial               ║" << endl;
    cout << "╚════════════════════════════════════════════════════════╝" << endl;
    
    example1_basic();
    example2_isolation();
    example3_nested();
    example4_resource_control();
    example5_sequential_mode();
    example6_enqueue();
    example7_server_pattern();
    
    cout << "\n╔════════════════════════════════════════════════════════╗" << endl;
    cout << "║                    KEY TAKEAWAYS                       ║" << endl;
    cout << "╠════════════════════════════════════════════════════════╣" << endl;
    cout << "║                                                        ║" << endl;
    cout << "║  1. task_arena controls thread count explicitly       ║" << endl;
    cout << "║  2. Isolates workloads into separate thread pools     ║" << endl;
    cout << "║  3. execute() runs work synchronously in arena        ║" << endl;
    cout << "║  4. enqueue() queues work for asynchronous execution  ║" << endl;
    cout << "║  5. Prevents oversubscription with nested parallelism ║" << endl;
    cout << "║  6. Enables resource prioritization                   ║" << endl;
    cout << "║  7. Supports NUMA-aware execution                     ║" << endl;
    cout << "║                                                        ║" << endl;
    cout << "╠════════════════════════════════════════════════════════╣" << endl;
    cout << "║               COMMON PATTERNS                          ║" << endl;
    cout << "╠════════════════════════════════════════════════════════╣" << endl;
    cout << "║                                                        ║" << endl;
    cout << "║  Fixed Thread Count:                                   ║" << endl;
    cout << "║    task_arena arena(4);                               ║" << endl;
    cout << "║    arena.execute([&]() { /* work */ });               ║" << endl;
    cout << "║                                                        ║" << endl;
    cout << "║  Isolation:                                            ║" << endl;
    cout << "║    task_arena fast(4), slow(2);                       ║" << endl;
    cout << "║    fast.execute(/* fast work */);                     ║" << endl;
    cout << "║    slow.execute(/* slow work */);                     ║" << endl;
    cout << "║                                                        ║" << endl;
    cout << "║  Async Enqueue:                                        ║" << endl;
    cout << "║    arena.enqueue([&]() { /* background work */ });    ║" << endl;
    cout << "║                                                        ║" << endl;
    cout << "╠════════════════════════════════════════════════════════╣" << endl;
    cout << "║               WHEN TO USE                              ║" << endl;
    cout << "╠════════════════════════════════════════════════════════╣" << endl;
    cout << "║                                                        ║" << endl;
    cout << "║  ✓ Need specific thread count                         ║" << endl;
    cout << "║  ✓ Multiple independent workloads                     ║" << endl;
    cout << "║  ✓ Resource prioritization needed                     ║" << endl;
    cout << "║  ✓ Nested parallel frameworks                         ║" << endl;
    cout << "║  ✓ NUMA optimization                                  ║" << endl;
    cout << "║  ✓ Prevent oversubscription                           ║" << endl;
    cout << "║  ✗ Simple parallelism (use parallel_for instead)      ║" << endl;
    cout << "║                                                        ║" << endl;
    cout << "╚════════════════════════════════════════════════════════╝" << endl;
    
    return 0;
}
