/*
 * TBB CONCURRENT_VECTOR - Thread-Safe Dynamic Array
 * 
 * DEFINITION:
 * concurrent_vector is a thread-safe version of std::vector that allows
 * concurrent growth and element access from multiple threads.
 * 
 * KEY FEATURES:
 * - Concurrent push_back operations
 * - Concurrent element access (read/write)
 * - Never invalidates iterators or references (unlike std::vector)
 * - Lock-free for most operations
 * - Grows automatically
 * 
 * WHEN TO USE:
 * - Multiple threads need to add elements
 * - Random access needed
 * - Don't know final size in advance
 * - Need vector-like interface with thread safety
 * 
 * DIFFERENCES FROM std::vector:
 * - Concurrent operations safe
 * - No reallocation (uses segments)
 * - Iterator stability guaranteed
 * - Slightly slower than vector for single-threaded use
 */

#include <iostream>
#include <cmath>
#include <vector>
#include <tbb/tbb.h>
#include <tbb/concurrent_vector.h>
#include <tbb/parallel_for.h>
#include <chrono>
#include <thread>

using namespace std;
using std::atomic;
using namespace tbb;
using namespace chrono;

// ============= Example 1: Basic Concurrent Push_back =============

void example1_basic_pushback() {
    cout << "\n=== Example 1: Concurrent Push_back ===" << endl;
    
    concurrent_vector<int> vec;
    const int N = 10000;
    const int NUM_THREADS = 4;
    
    // Multiple threads adding elements concurrently
    parallel_for(blocked_range<int>(0, NUM_THREADS),
        [&](blocked_range<int> r) {
            for(int t = r.begin(); t != r.end(); ++t) {
                for(int i = 0; i < N; ++i) {
                    vec.push_back(t * 10000 + i);
                }
            }
        });
    
    cout << "Added elements from " << NUM_THREADS << " threads" << endl;
    cout << "Total elements: " << vec.size() << endl;
    cout << "Expected: " << N * NUM_THREADS << endl;
    cout << "First 10: ";
    for(int i = 0; i < 10 && i < (int)vec.size(); ++i)
        cout << vec[i] << " ";
    cout << endl;
}

// ============= Example 2: Concurrent Read/Write Access =============

void example2_concurrent_access() {
    cout << "\n=== Example 2: Concurrent Read/Write Access ===" << endl;
    
    const int N = 100000;
    concurrent_vector<int> vec(N);
    
    // Initialize
    for(int i = 0; i < N; ++i) vec[i] = i;
    
    // Multiple threads reading and modifying concurrently
    parallel_for(blocked_range<int>(0, N),
        [&](blocked_range<int> r) {
            for(int i = r.begin(); i != r.end(); ++i) {
                // Read current value
                int val = vec[i];
                // Modify
                vec[i] = val * 2;
            }
        });
    
    cout << "Modified " << N << " elements concurrently" << endl;
    cout << "vec[100] = " << vec[100] << " (expected: 200)" << endl;
    cout << "vec[999] = " << vec[999] << " (expected: 1998)" << endl;
}

// ============= Example 3: grow_by and grow_to_at_least =============

void example3_grow_operations() {
    cout << "\n=== Example 3: Grow Operations ===" << endl;
    
    concurrent_vector<int> vec;
    
    // grow_by: Atomically add N elements
    auto old_size = vec.grow_by(100);
    cout << "After grow_by(100): size = " << vec.size() 
         << " (grow_by returns iterator)" << endl;
    
    // Initialize the new elements
    for(size_t i = old_size; i < vec.size(); ++i)
        vec[i] = i;
    
    // grow_to_at_least: Ensure minimum size
    vec.grow_to_at_least(500);
    cout << "After grow_to_at_least(500): size = " << vec.size() << endl;
    
    vec.grow_to_at_least(300);  // No effect, already larger
    cout << "After grow_to_at_least(300): size = " << vec.size() 
         << " (no change)" << endl;
    
    // grow_by with initializer
    old_size = vec.grow_by(10, 999);  // Add 10 elements, all set to 999
    cout << "After grow_by(10, 999): vec[" << old_size << "] = " 
         << vec[old_size] << endl;
}

// ============= Example 4: Iterator Stability =============

void example4_iterator_stability() {
    cout << "\n=== Example 4: Iterator Stability ===" << endl;
    
    concurrent_vector<int> vec;
    vec.push_back(1);
    vec.push_back(2);
    vec.push_back(3);
    
    // Get iterator and reference
    auto it = vec.begin();
    int& ref = vec[1];
    
    cout << "Before growth: *it = " << *it << ", ref = " << ref << endl;
    
    // Grow vector from another thread
    thread t([&]() {
        for(int i = 0; i < 10000; ++i)
            vec.push_back(i);
    });
    t.join();
    
    // Iterator and reference STILL VALID (unlike std::vector!)
    cout << "After growth: *it = " << *it << ", ref = " << ref << endl;
    cout << "Vector size: " << vec.size() << endl;
    cout << "✓ Iterators and references remain valid!" << endl;
}

// ============= Example 5: Parallel Population =============

struct WorkItem {
    int id;
    double result;
    string description;
    
    WorkItem() : id(0), result(0.0), description("") {}
    WorkItem(int i, double r, string d) 
        : id(i), result(r), description(d) {}
};

void example5_parallel_population() {
    cout << "\n=== Example 5: Parallel Population with Complex Types ===" << endl;
    
    const int N = 10000;
    concurrent_vector<WorkItem> results;
    
    auto start = high_resolution_clock::now();
    
    // Process work items in parallel and collect results
    parallel_for(blocked_range<int>(0, N),
        [&](blocked_range<int> r) {
            for(int i = r.begin(); i != r.end(); ++i) {
                // Simulate work
                double result = sin(i * 0.001) * cos(i * 0.002);
                
                // Add result (thread-safe)
                results.push_back(
                    WorkItem(i, result, "Task_" + to_string(i))
                );
            }
        });
    
    auto elapsed = duration_cast<milliseconds>(
        high_resolution_clock::now() - start).count();
    
    cout << "Processed " << results.size() << " work items in " 
         << elapsed << " ms" << endl;
    cout << "Sample result: id=" << results[100].id 
         << ", result=" << results[100].result << endl;
}

// ============= Example 6: Comparison with std::vector =============

void example6_performance_comparison() {
    cout << "\n=== Example 6: Performance Comparison ===" << endl;
    
    const int N = 1000000;
    
    // Sequential std::vector
    auto start = high_resolution_clock::now();
    vector<int> std_vec;
    for(int i = 0; i < N; ++i)
        std_vec.push_back(i);
    auto std_time = duration_cast<milliseconds>(
        high_resolution_clock::now() - start).count();
    
    // Sequential concurrent_vector
    start = high_resolution_clock::now();
    concurrent_vector<int> conc_vec_seq;
    for(int i = 0; i < N; ++i)
        conc_vec_seq.push_back(i);
    auto conc_seq_time = duration_cast<milliseconds>(
        high_resolution_clock::now() - start).count();
    
    // Parallel concurrent_vector
    start = high_resolution_clock::now();
    concurrent_vector<int> conc_vec_par;
    parallel_for(blocked_range<int>(0, N),
        [&](blocked_range<int> r) {
            for(int i = r.begin(); i != r.end(); ++i)
                conc_vec_par.push_back(i);
        });
    auto conc_par_time = duration_cast<milliseconds>(
        high_resolution_clock::now() - start).count();
    
    cout << "std::vector (sequential):         " << std_time << " ms" << endl;
    cout << "concurrent_vector (sequential):   " << conc_seq_time << " ms" << endl;
    cout << "concurrent_vector (parallel):     " << conc_par_time << " ms" << endl;
    cout << "\nNote: concurrent_vector slower sequentially but" << endl;
    cout << "      much faster with parallel insertions!" << endl;
}

// ============= Example 7: Real-world Use Case - Log Collection =============

struct LogEntry {
    int thread_id;
    string message;
    chrono::system_clock::time_point timestamp;
    
    LogEntry() : thread_id(0), message(""), 
                 timestamp(chrono::system_clock::now()) {}
    LogEntry(int tid, string msg) 
        : thread_id(tid), message(msg),
          timestamp(chrono::system_clock::now()) {}
};

void example7_log_collection() {
    cout << "\n=== Example 7: Thread-Safe Log Collection ===" << endl;
    
    concurrent_vector<LogEntry> logs;
    const int NUM_THREADS = 8;
    const int LOGS_PER_THREAD = 1000;
    
    // Multiple threads generating logs
    parallel_for(blocked_range<int>(0, NUM_THREADS),
        [&](blocked_range<int> r) {
            for(int t = r.begin(); t != r.end(); ++t) {
                for(int i = 0; i < LOGS_PER_THREAD; ++i) {
                    logs.push_back(LogEntry(
                        t,
                        "Thread " + to_string(t) + " message " + to_string(i)
                    ));
                    
                    // Simulate some work
                    this_thread::sleep_for(chrono::microseconds(1));
                }
            }
        });
    
    cout << "Collected " << logs.size() << " log entries" << endl;
    cout << "Expected: " << NUM_THREADS * LOGS_PER_THREAD << endl;
    cout << "\nFirst 5 logs:" << endl;
    for(int i = 0; i < 5 && i < (int)logs.size(); ++i) {
        cout << "  Thread " << logs[i].thread_id 
             << ": " << logs[i].message << endl;
    }
}

// ============= Main =============

int main() {
    cout << "╔════════════════════════════════════════════════════════╗" << endl;
    cout << "║     TBB concurrent_vector - Complete Tutorial         ║" << endl;
    cout << "╚════════════════════════════════════════════════════════╝" << endl;
    
    example1_basic_pushback();
    example2_concurrent_access();
    example3_grow_operations();
    example4_iterator_stability();
    example5_parallel_population();
    example6_performance_comparison();
    example7_log_collection();
    
    cout << "\n╔════════════════════════════════════════════════════════╗" << endl;
    cout << "║                    KEY TAKEAWAYS                       ║" << endl;
    cout << "╠════════════════════════════════════════════════════════╣" << endl;
    cout << "║                                                        ║" << endl;
    cout << "║  1. Thread-safe push_back and element access          ║" << endl;
    cout << "║  2. Never invalidates iterators/references            ║" << endl;
    cout << "║  3. Uses segmented storage (not contiguous)           ║" << endl;
    cout << "║  4. grow_by for atomic bulk insertion                 ║" << endl;
    cout << "║  5. Slower than std::vector for single thread         ║" << endl;
    cout << "║  6. Much faster with concurrent insertions            ║" << endl;
    cout << "║  7. Perfect for parallel result collection            ║" << endl;
    cout << "║  8. Use for thread-safe dynamic arrays                ║" << endl;
    cout << "║                                                        ║" << endl;
    cout << "╚════════════════════════════════════════════════════════╝" << endl;
    
    return 0;
}

