/*
 * TBB BLOCKED_RANGE - Iteration Space Concepts
 * 
 * DEFINITION:
 * blocked_range represents a one-dimensional iteration space that can be
 * recursively subdivided for parallel execution.
 * 
 * KEY CONCEPTS:
 * - begin(): First index in range
 * - end(): One past last index
 * - size(): Number of elements
 * - grain_size: Minimum subdivision size
 * - is_divisible(): Can range be split?
 * - empty(): Is range empty?
 * 
 * RANGE TYPES:
 * - blocked_range<T>: 1D range [begin, end)
 * - blocked_range2d<T>: 2D range (rows × cols)
 * - blocked_range3d<T>: 3D range (pages × rows × cols)
 * - Custom ranges: Implement Range concept
 * 
 * GRAIN SIZE:
 * Controls when subdivision stops. Trade-off:
 * - Too small: High overhead from too many tasks
 * - Too large: Poor load balancing
 * - Typical: 1000-10000 iterations
 * 
 * WHEN TO USE:
 * - Standard iteration patterns
 * - Index-based loops
 * - Multi-dimensional arrays
 * - Custom iteration spaces
 */

#include <iostream>
#include <cmath>
#include <vector>
#include <chrono>
#include <tbb/parallel_for.h>
#include <tbb/parallel_reduce.h>
#include <tbb/blocked_range.h>
#include <tbb/blocked_range2d.h>
#include <tbb/blocked_range3d.h>

using namespace std;
using namespace tbb;
using namespace chrono;

// ============= Example 1: Basic blocked_range =============

void example1_basic() {
    cout << "\n=== Example 1: Basic blocked_range ===" << endl;
    
    blocked_range<int> range(0, 100, 10);  // [0, 100) with grain 10
    
    cout << "Range [" << range.begin() << ", " << range.end() << ")" << endl;
    cout << "Size: " << range.size() << endl;
    cout << "Grain size: " << range.grainsize() << endl;
    cout << "Is divisible: " << (range.is_divisible() ? "yes" : "no") << endl;
    cout << "Is empty: " << (range.empty() ? "yes" : "no") << endl;
    
    // Splitting range
    blocked_range<int> left(range, split());
    cout << "\nAfter split:" << endl;
    cout << "Left:  [" << left.begin() << ", " << left.end() << ")" << endl;
    cout << "Right: [" << range.begin() << ", " << range.end() << ")" << endl;
}

// ============= Example 2: Grain Size Impact =============

void example2_grain_size() {
    cout << "\n=== Example 2: Grain Size Impact ===" << endl;
    
    const int N = 10000000;
    vector<int> data(N, 1);
    
    cout << "Testing different grain sizes on " << N << " elements:" << endl;
    
    for(int grain : {1, 100, 1000, 10000, 100000}) {
        auto start = high_resolution_clock::now();
        
        parallel_for(blocked_range<int>(0, N, grain),
            [&](blocked_range<int> r) {
                for(int i = r.begin(); i != r.end(); ++i) {
                    data[i] *= 2;
                }
            });
        
        auto time = duration_cast<milliseconds>(
            high_resolution_clock::now() - start).count();
        
        cout << "  Grain " << grain << ": " << time << " ms" << endl;
    }
    
    cout << "\nOptimal grain size balances overhead vs load balancing" << endl;
}

// ============= Example 3: blocked_range2d =============

void example3_2d_range() {
    cout << "\n=== Example 3: blocked_range2d ===" << endl;
    
    const int ROWS = 1000;
    const int COLS = 1000;
    vector<vector<double>> matrix(ROWS, vector<double>(COLS, 1.0));
    
    auto start = high_resolution_clock::now();
    
    // 2D parallel iteration
    parallel_for(blocked_range2d<int>(0, ROWS, 0, COLS),
        [&](blocked_range2d<int> r) {
            for(int i = r.rows().begin(); i != r.rows().end(); ++i) {
                for(int j = r.cols().begin(); j != r.cols().end(); ++j) {
                    matrix[i][j] = i * j;
                }
            }
        });
    
    auto time = duration_cast<milliseconds>(
        high_resolution_clock::now() - start).count();
    
    cout << "Processed " << ROWS << "x" << COLS << " matrix" << endl;
    cout << "Time: " << time << " ms" << endl;
    cout << "blocked_range2d splits in both dimensions" << endl;
}

// ============= Example 4: blocked_range3d =============

void example4_3d_range() {
    cout << "\n=== Example 4: blocked_range3d ===" << endl;
    
    const int PAGES = 100;
    const int ROWS = 100;
    const int COLS = 100;
    
    // 3D array
    vector<vector<vector<int>>> cube(PAGES,
        vector<vector<int>>(ROWS, vector<int>(COLS, 0)));
    
    auto start = high_resolution_clock::now();
    
    // 3D parallel iteration
    parallel_for(blocked_range3d<int>(0, PAGES, 0, ROWS, 0, COLS),
        [&](blocked_range3d<int> r) {
            for(int p = r.pages().begin(); p != r.pages().end(); ++p) {
                for(int i = r.rows().begin(); i != r.rows().end(); ++i) {
                    for(int j = r.cols().begin(); j != r.cols().end(); ++j) {
                        cube[p][i][j] = p + i + j;
                    }
                }
            }
        });
    
    auto time = duration_cast<milliseconds>(
        high_resolution_clock::now() - start).count();
    
    cout << "Processed " << PAGES << "x" << ROWS << "x" << COLS << " cube" << endl;
    cout << "Time: " << time << " ms" << endl;
    cout << "blocked_range3d for volumetric data" << endl;
}

// ============= Example 5: Custom Range Type =============

class CustomRange {
    size_t my_begin;
    size_t my_end;
    size_t my_grain;
    
public:
    CustomRange(size_t b, size_t e, size_t g = 1)
        : my_begin(b), my_end(e), my_grain(g) {}
    
    // Required: splitting constructor
    CustomRange(CustomRange& r, split)
        : my_begin(r.my_begin), my_grain(r.my_grain) {
        size_t mid = (r.my_begin + r.my_end) / 2;
        my_end = mid;
        r.my_begin = mid;
    }
    
    // Required methods
    size_t begin() const { return my_begin; }
    size_t end() const { return my_end; }
    size_t grainsize() const { return my_grain; }
    bool empty() const { return my_begin >= my_end; }
    bool is_divisible() const {
        return (my_end - my_begin) > my_grain;
    }
};

void example5_custom_range() {
    cout << "\n=== Example 5: Custom Range Type ===" << endl;
    
    vector<int> data(1000);
    
    parallel_for(CustomRange(0, data.size(), 100),
        [&](CustomRange r) {
            for(size_t i = r.begin(); i != r.end(); ++i) {
                data[i] = i * i;
            }
        });
    
    cout << "Processed with custom range" << endl;
    cout << "First 10 elements: ";
    for(int i = 0; i < 10; ++i) {
        cout << data[i] << " ";
    }
    cout << endl;
}

// ============= Example 6: Matrix Multiplication with 2D Range =============

void example6_matrix_multiply() {
    cout << "\n=== Example 6: Matrix Multiplication ===" << endl;
    
    const int N = 500;
    vector<vector<double>> A(N, vector<double>(N, 1.0));
    vector<vector<double>> B(N, vector<double>(N, 2.0));
    vector<vector<double>> C(N, vector<double>(N, 0.0));
    
    auto start = high_resolution_clock::now();
    
    parallel_for(blocked_range2d<int>(0, N, 0, N),
        [&](blocked_range2d<int> r) {
            for(int i = r.rows().begin(); i != r.rows().end(); ++i) {
                for(int j = r.cols().begin(); j != r.cols().end(); ++j) {
                    double sum = 0.0;
                    for(int k = 0; k < N; ++k) {
                        sum += A[i][k] * B[k][j];
                    }
                    C[i][j] = sum;
                }
            }
        });
    
    auto time = duration_cast<milliseconds>(
        high_resolution_clock::now() - start).count();
    
    cout << "Multiplied " << N << "x" << N << " matrices" << endl;
    cout << "Time: " << time << " ms" << endl;
    cout << "Result C[0][0] = " << C[0][0] << endl;
}

// ============= Example 7: Reduction with blocked_range =============

void example7_reduction() {
    cout << "\n=== Example 7: Reduction with blocked_range ===" << endl;
    
    const int N = 100000000;
    
    auto start = high_resolution_clock::now();
    
    long long sum = parallel_reduce(
        blocked_range<int>(0, N, 10000),  // Grain size 10000
        0LL,
        [](blocked_range<int> r, long long init) -> long long {
            for(int i = r.begin(); i != r.end(); ++i) {
                init += i;
            }
            return init;
        },
        [](long long x, long long y) -> long long {
            return x + y;
        });
    
    auto time = duration_cast<milliseconds>(
        high_resolution_clock::now() - start).count();
    
    cout << "Sum of 0.." << (N-1) << " = " << sum << endl;
    cout << "Time: " << time << " ms" << endl;
    cout << "Grain size affects number of reductions" << endl;
}

int main() {
    cout << "╔════════════════════════════════════════════════════════╗" << endl;
    cout << "║     TBB blocked_range - Complete Tutorial             ║" << endl;
    cout << "╚════════════════════════════════════════════════════════╝" << endl;
    
    example1_basic();
    example2_grain_size();
    example3_2d_range();
    example4_3d_range();
    example5_custom_range();
    example6_matrix_multiply();
    example7_reduction();
    
    cout << "\n╔════════════════════════════════════════════════════════╗" << endl;
    cout << "║                    KEY TAKEAWAYS                       ║" << endl;
    cout << "╠════════════════════════════════════════════════════════╣" << endl;
    cout << "║                                                        ║" << endl;
    cout << "║  1. blocked_range represents iteration space          ║" << endl;
    cout << "║  2. Grain size controls subdivision                   ║" << endl;
    cout << "║  3. blocked_range2d for 2D iteration                  ║" << endl;
    cout << "║  4. blocked_range3d for 3D iteration                  ║" << endl;
    cout << "║  5. Can create custom range types                     ║" << endl;
    cout << "║  6. Optimal grain: 1000-10000 iterations              ║" << endl;
    cout << "║  7. Too small grain = overhead                        ║" << endl;
    cout << "║  8. Too large grain = poor load balancing             ║" << endl;
    cout << "║                                                        ║" << endl;
    cout << "╠════════════════════════════════════════════════════════╣" << endl;
    cout << "║               USAGE PATTERNS                           ║" << endl;
    cout << "╠════════════════════════════════════════════════════════╣" << endl;
    cout << "║                                                        ║" << endl;
    cout << "║  1D Range:                                             ║" << endl;
    cout << "║    blocked_range<int> r(begin, end, grain);           ║" << endl;
    cout << "║    parallel_for(r, [](blocked_range<int> r) {         ║" << endl;
    cout << "║        for(int i = r.begin(); i != r.end(); ++i)      ║" << endl;
    cout << "║            process(i);                                 ║" << endl;
    cout << "║    });                                                 ║" << endl;
    cout << "║                                                        ║" << endl;
    cout << "║  2D Range:                                             ║" << endl;
    cout << "║    blocked_range2d<int> r(0, rows, 0, cols);          ║" << endl;
    cout << "║    parallel_for(r, [](blocked_range2d<int> r) {       ║" << endl;
    cout << "║        for(int i = r.rows().begin(); ...)             ║" << endl;
    cout << "║            for(int j = r.cols().begin(); ...)         ║" << endl;
    cout << "║    });                                                 ║" << endl;
    cout << "║                                                        ║" << endl;
    cout << "║  3D Range:                                             ║" << endl;
    cout << "║    blocked_range3d<int> r(0, p, 0, r, 0, c);          ║" << endl;
    cout << "║    Use .pages(), .rows(), .cols()                     ║" << endl;
    cout << "║                                                        ║" << endl;
    cout << "╠════════════════════════════════════════════════════════╣" << endl;
    cout << "║               GRAIN SIZE GUIDELINES                    ║" << endl;
    cout << "╠════════════════════════════════════════════════════════╣" << endl;
    cout << "║                                                        ║" << endl;
    cout << "║  Too Small (< 100):                                    ║" << endl;
    cout << "║    - Excessive task creation overhead                  ║" << endl;
    cout << "║    - Poor cache locality                               ║" << endl;
    cout << "║    - More synchronization                              ║" << endl;
    cout << "║                                                        ║" << endl;
    cout << "║  Optimal (1000-10000):                                 ║" << endl;
    cout << "║    - Good balance of overhead and load balancing       ║" << endl;
    cout << "║    - Reasonable cache locality                         ║" << endl;
    cout << "║    - Adaptive to workload                              ║" << endl;
    cout << "║                                                        ║" << endl;
    cout << "║  Too Large (> 100000):                                 ║" << endl;
    cout << "║    - Poor load balancing                               ║" << endl;
    cout << "║    - Underutilized threads                             ║" << endl;
    cout << "║    - Wasted parallelism potential                      ║" << endl;
    cout << "║                                                        ║" << endl;
    cout << "║  Adjust based on:                                      ║" << endl;
    cout << "║    - Work per iteration (more work = larger grain)     ║" << endl;
    cout << "║    - Total iterations (fewer iterations = larger grain)║" << endl;
    cout << "║    - Hardware (more cores = smaller grain)             ║" << endl;
    cout << "║                                                        ║" << endl;
    cout << "╠════════════════════════════════════════════════════════╣" << endl;
    cout << "║               WHEN TO USE                              ║" << endl;
    cout << "╠════════════════════════════════════════════════════════╣" << endl;
    cout << "║                                                        ║" << endl;
    cout << "║  blocked_range<T>:                                     ║" << endl;
    cout << "║    ✓ 1D index-based loops                              ║" << endl;
    cout << "║    ✓ Array processing                                  ║" << endl;
    cout << "║    ✓ Vector operations                                 ║" << endl;
    cout << "║                                                        ║" << endl;
    cout << "║  blocked_range2d<T>:                                   ║" << endl;
    cout << "║    ✓ 2D arrays/matrices                                ║" << endl;
    cout << "║    ✓ Image processing                                  ║" << endl;
    cout << "║    ✓ Matrix operations                                 ║" << endl;
    cout << "║                                                        ║" << endl;
    cout << "║  blocked_range3d<T>:                                   ║" << endl;
    cout << "║    ✓ 3D volumes                                        ║" << endl;
    cout << "║    ✓ Scientific simulations                            ║" << endl;
    cout << "║    ✓ Video processing                                  ║" << endl;
    cout << "║                                                        ║" << endl;
    cout << "║  Custom Range:                                         ║" << endl;
    cout << "║    ✓ Non-contiguous iteration                          ║" << endl;
    cout << "║    ✓ Tree traversals                                   ║" << endl;
    cout << "║    ✓ Custom data structures                            ║" << endl;
    cout << "║                                                        ║" << endl;
    cout << "╚════════════════════════════════════════════════════════╝" << endl;
    
    return 0;
}
