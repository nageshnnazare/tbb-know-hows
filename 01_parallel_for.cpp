/*
 * TBB PARALLEL_FOR - The Foundation of Parallel Loops
 * 
 * DEFINITION:
 * parallel_for applies a function to each element of a range in parallel.
 * It's the most commonly used TBB algorithm.
 * 
 * SYNTAX:
 * parallel_for(range, body, [partitioner])
 * 
 * WHEN TO USE:
 * - Independent iterations (no dependencies between loop iterations)
 * - CPU-bound operations
 * - Large enough grain size (>10μs per iteration block)
 * 
 * KEY CONCEPTS:
 * - blocked_range: Defines iteration space
 * - Grain size: Minimum number of iterations per task
 * - Partitioner: Controls how work is divided
 * - Body: Function/lambda to apply to each range
 */

#include <iostream>
#include <algorithm>
#include <vector>
#include <tbb/tbb.h>
#include <tbb/parallel_for.h>
#include <tbb/blocked_range.h>
#include <tbb/blocked_range2d.h>
#include <chrono>
#include <cmath>

using namespace std;
using namespace tbb;
using namespace chrono;

// ============= Example 1: Basic parallel_for =============

void example1_basic() {
    cout << "\n=== Example 1: Basic parallel_for ===" << endl;
    
    const int N = 100;
    vector<int> data(N);
    
    // Sequential version
    auto start = high_resolution_clock::now();
    for(int i = 0; i < N; ++i) {
        data[i] = i * i;
    }
    auto seq_time = duration_cast<microseconds>(
        high_resolution_clock::now() - start).count();
    
    // Parallel version with lambda
    fill(data.begin(), data.end(), 0);
    start = high_resolution_clock::now();
    parallel_for(blocked_range<int>(0, N),
        [&](blocked_range<int> r) {
            for(int i = r.begin(); i != r.end(); ++i) {
                data[i] = i * i;
            }
        });
    auto par_time = duration_cast<microseconds>(
        high_resolution_clock::now() - start).count();
    
    cout << "Sequential: " << seq_time << " μs" << endl;
    cout << "Parallel:   " << par_time << " μs" << endl;
    cout << "First 10 elements: ";
    for(int i = 0; i < 10; ++i) cout << data[i] << " ";
    cout << endl;
}

// ============= Example 2: Custom grain size =============

void example2_grain_size() {
    cout << "\n=== Example 2: Grain Size Control ===" << endl;
    
    const int N = 10000;
    vector<double> data(N);
    
    // Small grain size (more parallelism, more overhead)
    auto start = high_resolution_clock::now();
    parallel_for(blocked_range<int>(0, N, 10),  // grain_size = 10
        [&](blocked_range<int> r) {
            for(int i = r.begin(); i != r.end(); ++i) {
                data[i] = sin(i) * cos(i);
            }
        });
    auto small_grain = duration_cast<microseconds>(
        high_resolution_clock::now() - start).count();
    
    // Large grain size (less parallelism, less overhead)
    start = high_resolution_clock::now();
    parallel_for(blocked_range<int>(0, N, 1000),  // grain_size = 1000
        [&](blocked_range<int> r) {
            for(int i = r.begin(); i != r.end(); ++i) {
                data[i] = sin(i) * cos(i);
            }
        });
    auto large_grain = duration_cast<microseconds>(
        high_resolution_clock::now() - start).count();
    
    cout << "Small grain (10):   " << small_grain << " μs" << endl;
    cout << "Large grain (1000): " << large_grain << " μs" << endl;
    cout << "Rule of thumb: grain size should give 10-100μs of work" << endl;
}

// ============= Example 3: 2D parallel_for (Image Processing) =============

void example3_2d_parallel() {
    cout << "\n=== Example 3: 2D Parallel For (Image Processing) ===" << endl;
    
    const int HEIGHT = 1000;
    const int WIDTH = 1000;
    vector<vector<int>> image(HEIGHT, vector<int>(WIDTH, 0));
    vector<vector<int>> result(HEIGHT, vector<int>(WIDTH, 0));
    
    // Initialize image with some pattern
    for(int i = 0; i < HEIGHT; ++i)
        for(int j = 0; j < WIDTH; ++j)
            image[i][j] = i + j;
    
    // Parallel 2D processing (e.g., blur filter)
    auto start = high_resolution_clock::now();
    parallel_for(blocked_range2d<int>(0, HEIGHT, 0, WIDTH),
        [&](blocked_range2d<int> r) {
            for(int i = r.rows().begin(); i != r.rows().end(); ++i) {
                for(int j = r.cols().begin(); j != r.cols().end(); ++j) {
                    // Simple 3x3 average filter
                    int sum = 0;
                    int count = 0;
                    for(int di = -1; di <= 1; ++di) {
                        for(int dj = -1; dj <= 1; ++dj) {
                            int ni = i + di;
                            int nj = j + dj;
                            if(ni >= 0 && ni < HEIGHT && nj >= 0 && nj < WIDTH) {
                                sum += image[ni][nj];
                                count++;
                            }
                        }
                    }
                    result[i][j] = sum / count;
                }
            }
        });
    auto elapsed = duration_cast<milliseconds>(
        high_resolution_clock::now() - start).count();
    
    cout << "Processed " << HEIGHT << "x" << WIDTH << " image in " 
         << elapsed << " ms" << endl;
    cout << "Pixel (500,500): " << image[500][500] 
         << " -> " << result[500][500] << endl;
}

// ============= Example 4: Using Function Object =============

class ApplyTransform {
    vector<double>& data;
    double factor;
public:
    ApplyTransform(vector<double>& d, double f) : data(d), factor(f) {}
    
    void operator()(const blocked_range<int>& r) const {
        for(int i = r.begin(); i != r.end(); ++i) {
            data[i] = data[i] * factor + sin(data[i]);
        }
    }
};

void example4_function_object() {
    cout << "\n=== Example 4: Function Object ===" << endl;
    
    const int N = 1000;
    vector<double> data(N);
    for(int i = 0; i < N; ++i) data[i] = i * 0.01;
    
    parallel_for(blocked_range<int>(0, N),
                 ApplyTransform(data, 2.5));
    
    cout << "Applied transformation using function object" << endl;
    cout << "Sample values: " << data[0] << ", " << data[100] 
         << ", " << data[500] << endl;
}

// ============= Example 5: Simple parallel_for (range-based) =============

void example5_simple_syntax() {
    cout << "\n=== Example 5: Simple Syntax ===" << endl;
    
    const int N = 100;
    vector<int> data(N);
    
    // Simplified syntax (C++17)
    parallel_for(0, N, [&](int i) {
        data[i] = i * i * i;
    });
    
    cout << "Simplified parallel_for syntax" << endl;
    cout << "First 10 cubes: ";
    for(int i = 0; i < 10; ++i) cout << data[i] << " ";
    cout << endl;
}

// ============= Example 6: Partitioner Comparison =============

void expensive_computation(double& value, int iterations) {
    for(int i = 0; i < iterations; ++i) {
        value = sin(value) * cos(value) + sqrt(abs(value) + 1);
    }
}

void example6_partitioners() {
    cout << "\n=== Example 6: Partitioners ===" << endl;
    
    const int N = 1000;
    vector<double> data(N);
    for(int i = 0; i < N; ++i) data[i] = i;
    
    // auto_partitioner (default) - automatic grain size
    fill(data.begin(), data.end(), 1.0);
    auto start = high_resolution_clock::now();
    parallel_for(blocked_range<int>(0, N),
        [&](blocked_range<int> r) {
            for(int i = r.begin(); i != r.end(); ++i)
                expensive_computation(data[i], 100);
        },
        auto_partitioner());
    auto auto_time = duration_cast<milliseconds>(
        high_resolution_clock::now() - start).count();
    
    // static_partitioner - fixed grain size, no work stealing
    fill(data.begin(), data.end(), 1.0);
    start = high_resolution_clock::now();
    parallel_for(blocked_range<int>(0, N),
        [&](blocked_range<int> r) {
            for(int i = r.begin(); i != r.end(); ++i)
                expensive_computation(data[i], 100);
        },
        static_partitioner());
    auto static_time = duration_cast<milliseconds>(
        high_resolution_clock::now() - start).count();
    
    // simple_partitioner - deterministic splitting
    fill(data.begin(), data.end(), 1.0);
    start = high_resolution_clock::now();
    parallel_for(blocked_range<int>(0, N),
        [&](blocked_range<int> r) {
            for(int i = r.begin(); i != r.end(); ++i)
                expensive_computation(data[i], 100);
        },
        simple_partitioner());
    auto simple_time = duration_cast<milliseconds>(
        high_resolution_clock::now() - start).count();
    
    cout << "auto_partitioner:   " << auto_time << " ms" << endl;
    cout << "static_partitioner: " << static_time << " ms" << endl;
    cout << "simple_partitioner: " << simple_time << " ms" << endl;
    
    cout << "\nPartitioner Guide:" << endl;
    cout << "• auto (default): Best for most cases" << endl;
    cout << "• static: Uniform work, minimize overhead" << endl;
    cout << "• simple: Deterministic, reproducible results" << endl;
    cout << "• affinity: Cache locality important" << endl;
}

// ============= Example 7: Real-world Monte Carlo Simulation =============

void example7_monte_carlo() {
    cout << "\n=== Example 7: Monte Carlo Pi Estimation ===" << endl;
    
    const int N = 10000000;
    vector<int> hits(N);
    
    auto start = high_resolution_clock::now();
    parallel_for(blocked_range<int>(0, N),
        [&](blocked_range<int> r) {
            unsigned int seed = r.begin();  // Different seed per range
            for(int i = r.begin(); i != r.end(); ++i) {
                double x = (double)rand_r(&seed) / RAND_MAX;
                double y = (double)rand_r(&seed) / RAND_MAX;
                hits[i] = (x*x + y*y <= 1.0) ? 1 : 0;
            }
        });
    
    long long total_hits = 0;
    for(int h : hits) total_hits += h;
    double pi_estimate = 4.0 * total_hits / N;
    
    auto elapsed = duration_cast<milliseconds>(
        high_resolution_clock::now() - start).count();
    
    cout << "Samples: " << N << endl;
    cout << "Pi estimate: " << pi_estimate << endl;
    cout << "Error: " << abs(pi_estimate - 3.14159265359) << endl;
    cout << "Time: " << elapsed << " ms" << endl;
}

// ============= Main =============

int main() {
    cout << "╔════════════════════════════════════════════════════════╗" << endl;
    cout << "║       TBB parallel_for - Complete Tutorial             ║" << endl;
    cout << "╚════════════════════════════════════════════════════════╝" << endl;
    
    cout << "\nTBB Version: " << TBB_VERSION_MAJOR << "." << TBB_VERSION_MINOR << endl;
    cout << "Hardware threads: " << task_scheduler_init::default_num_threads() << endl;
    
    example1_basic();
    example2_grain_size();
    example3_2d_parallel();
    example4_function_object();
    example5_simple_syntax();
    example6_partitioners();
    example7_monte_carlo();
    
    cout << "\n";
    cout << "╔════════════════════════════════════════════════════════╗" << endl;
    cout << "║                    KEY TAKEAWAYS                       ║" << endl;
    cout << "╠════════════════════════════════════════════════════════╣" << endl;
    cout << "║                                                        ║" << endl;
    cout << "║  1. parallel_for is for independent loop iterations    ║" << endl;
    cout << "║  2. Use blocked_range to define iteration space        ║" << endl;
    cout << "║  3. Grain size controls parallelism vs overhead        ║" << endl;
    cout << "║  4. auto_partitioner is best for most cases            ║" << endl;
    cout << "║  5. 2D ranges available via blocked_range2d            ║" << endl;
    cout << "║  6. Lambda syntax is clean and convenient              ║" << endl;
    cout << "║  7. Function objects give more control                 ║" << endl;
    cout << "║  8. Ensure enough work per task (>10μs)                ║" << endl;
    cout << "║                                                        ║" << endl;
    cout << "╠════════════════════════════════════════════════════════╣" << endl;
    cout << "║               COMMON PATTERNS                          ║" << endl;
    cout << "╠════════════════════════════════════════════════════════╣" << endl;
    cout << "║                                                        ║" << endl;
    cout << "║  Array Processing:                                     ║" << endl;
    cout << "║    parallel_for(blocked_range<int>(0, N),              ║" << endl;
    cout << "║      [&](auto r) { for(int i : r) process(arr[i]); })  ║" << endl;
    cout << "║                                                        ║" << endl;
    cout << "║  Image Processing:                                     ║" << endl;
    cout << "║    parallel_for(blocked_range2d<int>(0, H, 0, W),      ║" << endl;
    cout << "║      [&](auto r) { /* process pixels */ });            ║" << endl;
    cout << "║                                                        ║" << endl;
    cout << "║  Simple Range:                                         ║" << endl;
    cout << "║    parallel_for(0, N, [&](int i) { work(i); });        ║" << endl;
    cout << "║                                                        ║" << endl;
    cout << "╠════════════════════════════════════════════════════════╣" << endl;
    cout << "║               PERFORMANCE TIPS                         ║" << endl;
    cout << "╠════════════════════════════════════════════════════════╣" << endl;
    cout << "║                                                        ║" << endl;
    cout << "║  ✓ Make grain size large enough (10-100μs)             ║" << endl;
    cout << "║  ✓ Avoid shared mutable state                          ║" << endl;
    cout << "║  ✓ Use local variables in loop body                    ║" << endl;
    cout << "║  ✓ Minimize synchronization                            ║" << endl;
    cout << "║  ✓ Let TBB handle thread count                         ║" << endl;
    cout << "║  ✓ Profile to find optimal grain size                  ║" << endl;
    cout << "║                                                        ║" << endl;
    cout << "╚════════════════════════════════════════════════════════╝" << endl;
    
    return 0;
}

