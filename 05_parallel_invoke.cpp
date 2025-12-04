/*
 * TBB PARALLEL_INVOKE - Execute Multiple Functions in Parallel
 * 
 * DEFINITION:
 * parallel_invoke executes multiple function objects in parallel
 * and waits for all to complete.
 * 
 * USE CASES:
 * - Independent tasks that should run concurrently
 * - Divide-and-conquer algorithms
 * - Fork-join patterns
 * - Parallel initialization
 */

#include <iostream>
#include <vector>
#include <algorithm>
#include <tbb/parallel_invoke.h>
#include <chrono>
#include <thread>
#include <cmath>

using namespace std;
using namespace tbb;
using namespace chrono;

// ============= Example 1: Basic parallel_invoke =============

void task1() {
    cout << "Task 1 started" << endl;
    this_thread::sleep_for(milliseconds(100));
    cout << "Task 1 completed" << endl;
}

void task2() {
    cout << "Task 2 started" << endl;
    this_thread::sleep_for(milliseconds(100));
    cout << "Task 2 completed" << endl;
}

void task3() {
    cout << "Task 3 started" << endl;
    this_thread::sleep_for(milliseconds(100));
    cout << "Task 3 completed" << endl;
}

void example1_basic() {
    cout << "\n=== Example 1: Basic parallel_invoke ===" << endl;
    
    auto start = high_resolution_clock::now();
    
    parallel_invoke(
        task1,
        task2,
        task3
    );
    
    auto elapsed = duration_cast<milliseconds>(
        high_resolution_clock::now() - start).count();
    
    cout << "All tasks completed in " << elapsed << " ms" << endl;
    cout << "(Should be ~100ms if parallel, ~300ms if sequential)" << endl;
}

// ============= Example 2: Lambda Functions =============

void example2_lambdas() {
    cout << "\n=== Example 2: Using Lambdas ===" << endl;
    
    int result1 = 0, result2 = 0, result3 = 0;
    
    parallel_invoke(
        [&]() {
            result1 = 0;
            for(int i = 0; i < 1000000; ++i) result1 += i;
        },
        [&]() {
            result2 = 1;
            for(int i = 1; i <= 20; ++i) result2 *= i;
        },
        [&]() {
            result3 = 0;
            for(int i = 0; i < 1000; ++i) result3 += i * i;
        }
    );
    
    cout << "Result 1 (sum): " << result1 << endl;
    cout << "Result 2 (factorial 20 mod INT_MAX): " << result2 << endl;
    cout << "Result 3 (sum of squares): " << result3 << endl;
}

// ============= Example 3: Parallel Quicksort (Divide & Conquer) =============

template<typename T>
void parallel_quicksort(vector<T>& data, int left, int right) {
    if(left >= right) return;
    
    // Partition
    T pivot = data[(left + right) / 2];
    int i = left, j = right;
    
    while(i <= j) {
        while(data[i] < pivot) i++;
        while(data[j] > pivot) j--;
        if(i <= j) {
            swap(data[i], data[j]);
            i++;
            j--;
        }
    }
    
    // Recursively sort in parallel
    if(right - left > 1000) {  // Parallel threshold
        parallel_invoke(
            [&]() { parallel_quicksort(data, left, j); },
            [&]() { parallel_quicksort(data, i, right); }
        );
    } else {  // Sequential for small ranges
        parallel_quicksort(data, left, j);
        parallel_quicksort(data, i, right);
    }
}

void example3_quicksort() {
    cout << "\n=== Example 3: Parallel Quicksort ===" << endl;
    
    const int N = 100000;
    vector<int> data(N);
    for(int i = 0; i < N; ++i) data[i] = rand() % 10000;
    
    auto start = high_resolution_clock::now();
    parallel_quicksort(data, 0, N - 1);
    auto elapsed = duration_cast<milliseconds>(
        high_resolution_clock::now() - start).count();
    
    bool sorted = is_sorted(data.begin(), data.end());
    cout << "Sorted " << N << " elements in " << elapsed << " ms" << endl;
    cout << "Verification: " << (sorted ? "✓ SORTED" : "✗ FAILED") << endl;
}

// ============= Example 4: Data Processing Pipeline =============

void example4_pipeline() {
    cout << "\n=== Example 4: Data Processing Pipeline ===" << endl;
    
    vector<int> raw_data(10000);
    vector<int> processed_data(10000);
    vector<int> aggregated_data(10);
    
    for(int i = 0; i < 10000; ++i) raw_data[i] = i;
    
    auto start = high_resolution_clock::now();
    
    parallel_invoke(
        // Stage 1: Process raw data
        [&]() {
            for(size_t i = 0; i < raw_data.size(); ++i)
                processed_data[i] = raw_data[i] * 2 + 1;
        },
        
        // Stage 2: Independent computation
        [&]() {
            for(size_t i = 0; i < aggregated_data.size(); ++i) {
                int sum = 0;
                for(int j = 0; j < 1000; ++j)
                    sum += j;
                aggregated_data[i] = sum;
            }
        }
    );
    
    auto elapsed = duration_cast<microseconds>(
        high_resolution_clock::now() - start).count();
    
    cout << "Pipeline completed in " << elapsed << " μs" << endl;
    cout << "Sample: processed[100] = " << processed_data[100] << endl;
}

// ============= Example 5: Nested parallel_invoke =============

void example5_nested() {
    cout << "\n=== Example 5: Nested parallel_invoke ===" << endl;
    
    int results[4] = {0, 0, 0, 0};
    
    parallel_invoke(
        [&]() {
            // First branch
            parallel_invoke(
                [&]() { results[0] = 1 * 1000; },
                [&]() { results[1] = 2 * 1000; }
            );
        },
        [&]() {
            // Second branch
            parallel_invoke(
                [&]() { results[2] = 3 * 1000; },
                [&]() { results[3] = 4 * 1000; }
            );
        }
    );
    
    cout << "Results: ";
    for(int r : results) cout << r << " ";
    cout << endl;
    cout << "All 4 branches can execute in parallel!" << endl;
}

int main() {
    cout << "╔════════════════════════════════════════════════════════╗" << endl;
    cout << "║     TBB parallel_invoke - Complete Tutorial           ║" << endl;
    cout << "╚════════════════════════════════════════════════════════╝" << endl;
    
    example1_basic();
    example2_lambdas();
    example3_quicksort();
    example4_pipeline();
    example5_nested();
    
    cout << "\n╔════════════════════════════════════════════════════════╗" << endl;
    cout << "║                    KEY TAKEAWAYS                       ║" << endl;
    cout << "╠════════════════════════════════════════════════════════╣" << endl;
    cout << "║                                                        ║" << endl;
    cout << "║  1. parallel_invoke runs multiple functions in parallel║" << endl;
    cout << "║  2. Waits for ALL functions to complete               ║" << endl;
    cout << "║  3. Perfect for independent tasks                      ║" << endl;
    cout << "║  4. Great for divide-and-conquer algorithms            ║" << endl;
    cout << "║  5. Can be nested for complex parallelism              ║" << endl;
    cout << "║  6. Works with functions, lambdas, functors            ║" << endl;
    cout << "║                                                        ║" << endl;
    cout << "╚════════════════════════════════════════════════════════╝" << endl;
    
    return 0;
}

