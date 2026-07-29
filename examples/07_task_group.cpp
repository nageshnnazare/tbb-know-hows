/*
 * TBB TASK_GROUP - Fine-Grained Task Management
 * 
 * DEFINITION:
 * task_group provides explicit control over task creation and synchronization.
 * Lower-level than parallel algorithms but more flexible.
 * 
 * USE CASES:
 * - Dynamic task creation
 * - Irregular parallelism
 * - When you need explicit control
 * - Recursive algorithms with varying depth
 */

#include <iostream>
#include <cmath>
#include <vector>
#include <tbb/task_group.h>
#include <chrono>

using namespace std;
using namespace tbb;
using namespace chrono;

// ============= Example 1: Basic Task Group =============

void example1_basic() {
    cout << "\n=== Example 1: Basic Task Group ===" << endl;
    
    task_group tg;
    
    int result1 = 0, result2 = 0, result3 = 0;
    
    tg.run([&]() { result1 = 100; });
    tg.run([&]() { result2 = 200; });
    tg.run([&]() { result3 = 300; });
    
    tg.wait();  // Wait for all tasks to complete
    
    cout << "Results: " << result1 << ", " << result2 << ", " << result3 << endl;
}

// ============= Example 2: Fibonacci with Task Group =============

int fib_sequential(int n) {
    if(n < 2) return n;
    return fib_sequential(n-1) + fib_sequential(n-2);
}

int fib_parallel(int n) {
    if(n < 20) return fib_sequential(n);  // Sequential threshold
    
    int x, y;
    task_group tg;
    
    tg.run([&]() { x = fib_parallel(n-1); });
    y = fib_parallel(n-2);  // Compute inline
    
    tg.wait();
    return x + y;
}

void example2_fibonacci() {
    cout << "\n=== Example 2: Parallel Fibonacci ===" << endl;
    
    const int N = 30;
    
    auto start = high_resolution_clock::now();
    int result_seq = fib_sequential(N);
    auto seq_time = duration_cast<milliseconds>(
        high_resolution_clock::now() - start).count();
    
    start = high_resolution_clock::now();
    int result_par = fib_parallel(N);
    auto par_time = duration_cast<milliseconds>(
        high_resolution_clock::now() - start).count();
    
    cout << "fib(" << N << ") = " << result_par << endl;
    cout << "Sequential: " << seq_time << " ms" << endl;
    cout << "Parallel:   " << par_time << " ms" << endl;
    cout << "Speedup:    " << (double)seq_time / par_time << "x" << endl;
}

// ============= Example 3: Dynamic Task Creation =============

void example3_dynamic() {
    cout << "\n=== Example 3: Dynamic Task Creation ===" << endl;
    
    vector<int> data = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
    vector<int> results(data.size());
    
    task_group tg;
    
    // Create task for each element dynamically
    for(size_t i = 0; i < data.size(); ++i) {
        tg.run([&, i]() {
            results[i] = data[i] * data[i];
        });
    }
    
    tg.wait();
    
    cout << "Squared values: ";
    for(int r : results) cout << r << " ";
    cout << endl;
}

int main() {
    cout << "╔════════════════════════════════════════════════════════╗" << endl;
    cout << "║      TBB task_group - Complete Tutorial               ║" << endl;
    cout << "╚════════════════════════════════════════════════════════╝" << endl;
    
    example1_basic();
    example2_fibonacci();
    example3_dynamic();
    
    cout << "\n╔════════════════════════════════════════════════════════╗" << endl;
    cout << "║                    KEY TAKEAWAYS                       ║" << endl;
    cout << "╠════════════════════════════════════════════════════════╣" << endl;
    cout << "║  1. task_group provides explicit task management       ║" << endl;
    cout << "║  2. run() adds tasks, wait() waits for completion      ║" << endl;
    cout << "║  3. Good for dynamic/irregular parallelism             ║" << endl;
    cout << "║  4. Lower-level than parallel_for/reduce              ║" << endl;
    cout << "╚════════════════════════════════════════════════════════╝" << endl;
    
    return 0;
}
