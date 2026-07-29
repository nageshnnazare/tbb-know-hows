/*
 * TBB PARALLEL_REDUCE - Parallel Aggregation and Reductions
 * 
 * DEFINITION:
 * parallel_reduce combines values from a range using an associative operation.
 * It's used for computing sums, products, min/max, and other aggregations.
 * 
 * SYNTAX:
 * result = parallel_reduce(range, identity, reduction_body, join_body)
 * 
 * WHEN TO USE:
 * - Computing aggregates (sum, product, min, max)
 * - Finding elements (first match, best match)
 * - Statistical computations (mean, variance)
 * - Any associative reduction operation
 * 
 * KEY CONCEPTS:
 * - Identity value: Starting value for reduction
 * - Reduction operator: Combines two values
 * - Splitting: Range is split for parallel processing
 * - Joining: Partial results are combined
 */

#include <iostream>
#include <vector>
#include <tbb/tbb.h>
#include <tbb/parallel_reduce.h>
#include <tbb/blocked_range.h>
#include <chrono>
#include <cmath>
#include <algorithm>
#include <numeric>

using namespace std;
using namespace tbb;
using namespace chrono;

// ============= Example 1: Simple Sum Reduction =============

void example1_simple_sum() {
    cout << "\n=== Example 1: Simple Sum ===" << endl;
    
    const int N = 10000000;
    vector<int> data(N);
    for(int i = 0; i < N; ++i) data[i] = i + 1;
    
    // Sequential sum
    auto start = high_resolution_clock::now();
    long long seq_sum = 0;
    for(int x : data) seq_sum += x;
    auto seq_time = duration_cast<milliseconds>(
        high_resolution_clock::now() - start).count();
    
    // Parallel sum
    start = high_resolution_clock::now();
    long long par_sum = parallel_reduce(
        blocked_range<int>(0, N),
        0LL,  // Identity value
        [&](blocked_range<int> r, long long init) -> long long {
            for(int i = r.begin(); i != r.end(); ++i)
                init += data[i];
            return init;
        },
        [](long long x, long long y) -> long long {
            return x + y;  // Join operation
        }
    );
    auto par_time = duration_cast<milliseconds>(
        high_resolution_clock::now() - start).count();
    
    cout << "Sequential sum: " << seq_sum << " (" << seq_time << " ms)" << endl;
    cout << "Parallel sum:   " << par_sum << " (" << par_time << " ms)" << endl;
    cout << "Speedup: " << (double)seq_time / par_time << "x" << endl;
}

// ============= Example 2: Finding Minimum Value =============

void example2_minimum() {
    cout << "\n=== Example 2: Finding Minimum ===" << endl;
    
    const int N = 5000000;
    vector<double> data(N);
    for(int i = 0; i < N; ++i) 
        data[i] = sin(i * 0.001) * 1000 + i;
    
    double min_val = parallel_reduce(
        blocked_range<int>(0, N),
        numeric_limits<double>::max(),  // Identity
        [&](blocked_range<int> r, double current_min) -> double {
            for(int i = r.begin(); i != r.end(); ++i)
                current_min = min(current_min, data[i]);
            return current_min;
        },
        [](double x, double y) -> double {
            return min(x, y);  // Join
        }
    );
    
    // Verify with std::min_element
    double std_min = *min_element(data.begin(), data.end());
    
    cout << "Parallel minimum: " << min_val << endl;
    cout << "STL minimum:      " << std_min << endl;
    cout << "Match: " << (abs(min_val - std_min) < 1e-6 ? "✓" : "✗") << endl;
}

// ============= Example 3: Dot Product =============

void example3_dot_product() {
    cout << "\n=== Example 3: Dot Product ===" << endl;
    
    const int N = 1000000;
    vector<double> a(N), b(N);
    for(int i = 0; i < N; ++i) {
        a[i] = i * 0.01;
        b[i] = (N - i) * 0.01;
    }
    
    double dot = parallel_reduce(
        blocked_range<int>(0, N),
        0.0,
        [&](blocked_range<int> r, double sum) -> double {
            for(int i = r.begin(); i != r.end(); ++i)
                sum += a[i] * b[i];
            return sum;
        },
        plus<double>()  // Can use standard function object
    );
    
    // Verify with sequential version
    double seq_dot = inner_product(a.begin(), a.end(), b.begin(), 0.0);
    
    cout << "Parallel dot product: " << dot << endl;
    cout << "Sequential (STL):     " << seq_dot << endl;
    cout << "Difference: " << abs(dot - seq_dot) << endl;
}

// ============= Example 4: Finding with Custom Criterion =============

struct FindResult {
    int index;
    double value;
    
    FindResult() : index(-1), value(numeric_limits<double>::lowest()) {}
    FindResult(int i, double v) : index(i), value(v) {}
};

void example4_custom_find() {
    cout << "\n=== Example 4: Find Maximum with Index ===" << endl;
    
    const int N = 1000000;
    vector<double> data(N);
    for(int i = 0; i < N; ++i)
        data[i] = sin(i * 0.001) * cos(i * 0.002) * 1000;
    
    FindResult result = parallel_reduce(
        blocked_range<int>(0, N),
        FindResult(),  // Identity
        [&](blocked_range<int> r, FindResult current) -> FindResult {
            for(int i = r.begin(); i != r.end(); ++i) {
                if(data[i] > current.value) {
                    current.index = i;
                    current.value = data[i];
                }
            }
            return current;
        },
        [](FindResult a, FindResult b) -> FindResult {
            return (a.value > b.value) ? a : b;  // Join
        }
    );
    
    cout << "Maximum value: " << result.value << endl;
    cout << "At index: " << result.index << endl;
    cout << "Verification: data[" << result.index << "] = " 
         << data[result.index] << endl;
}

// ============= Example 5: Statistical Computation (Mean & Variance) =============

struct Stats {
    double sum;
    double sum_sq;
    int count;
    
    Stats() : sum(0), sum_sq(0), count(0) {}
    Stats(double s, double sq, int c) : sum(s), sum_sq(sq), count(c) {}
    
    double mean() const { return sum / count; }
    double variance() const {
        double m = mean();
        return (sum_sq / count) - (m * m);
    }
};

void example5_statistics() {
    cout << "\n=== Example 5: Mean and Variance ===" << endl;
    
    const int N = 10000000;
    vector<double> data(N);
    for(int i = 0; i < N; ++i)
        data[i] = sin(i * 0.0001) * 100 + 50;
    
    Stats stats = parallel_reduce(
        blocked_range<int>(0, N),
        Stats(),
        [&](blocked_range<int> r, Stats s) -> Stats {
            for(int i = r.begin(); i != r.end(); ++i) {
                s.sum += data[i];
                s.sum_sq += data[i] * data[i];
                s.count++;
            }
            return s;
        },
        [](Stats a, Stats b) -> Stats {
            return Stats(a.sum + b.sum, 
                        a.sum_sq + b.sum_sq,
                        a.count + b.count);
        }
    );
    
    cout << "Sample size: " << stats.count << endl;
    cout << "Mean: " << stats.mean() << endl;
    cout << "Variance: " << stats.variance() << endl;
    cout << "Std deviation: " << sqrt(stats.variance()) << endl;
}

// ============= Example 6: Using Deterministic Reduce =============

void example6_deterministic() {
    cout << "\n=== Example 6: Deterministic Reduce ===" << endl;
    
    const int N = 1000000;
    vector<double> data(N);
    for(int i = 0; i < N; ++i) data[i] = i * 0.001;
    
    // Regular parallel_reduce (non-deterministic due to floating point)
    double sum1 = parallel_reduce(
        blocked_range<int>(0, N), 0.0,
        [&](blocked_range<int> r, double s) {
            for(int i = r.begin(); i != r.end(); ++i) s += data[i];
            return s;
        },
        plus<double>());
    
    double sum2 = parallel_reduce(
        blocked_range<int>(0, N), 0.0,
        [&](blocked_range<int> r, double s) {
            for(int i = r.begin(); i != r.end(); ++i) s += data[i];
            return s;
        },
        plus<double>());
    
    // With deterministic_reduce (guarantees same result)
    double sum3 = parallel_deterministic_reduce(
        blocked_range<int>(0, N), 0.0,
        [&](blocked_range<int> r, double s) {
            for(int i = r.begin(); i != r.end(); ++i) s += data[i];
            return s;
        },
        plus<double>());
    
    double sum4 = parallel_deterministic_reduce(
        blocked_range<int>(0, N), 0.0,
        [&](blocked_range<int> r, double s) {
            for(int i = r.begin(); i != r.end(); ++i) s += data[i];
            return s;
        },
        plus<double>());
    
    cout << "parallel_reduce run 1:        " << sum1 << endl;
    cout << "parallel_reduce run 2:        " << sum2 << endl;
    cout << "deterministic_reduce run 1:   " << sum3 << endl;
    cout << "deterministic_reduce run 2:   " << sum4 << endl;
    cout << "\nNote: Deterministic version guarantees repeatability" << endl;
}

// ============= Example 7: Complex Reduction (Histogram) =============

void example7_histogram() {
    cout << "\n=== Example 7: Parallel Histogram ===" << endl;
    
    const int N = 10000000;
    const int BINS = 10;
    vector<double> data(N);
    for(int i = 0; i < N; ++i)
        data[i] = (double)rand() / RAND_MAX * 100;  // 0-100
    
    vector<int> histogram = parallel_reduce(
        blocked_range<int>(0, N),
        vector<int>(BINS, 0),  // Identity: empty histogram
        [&](blocked_range<int> r, vector<int> hist) -> vector<int> {
            for(int i = r.begin(); i != r.end(); ++i) {
                int bin = min((int)(data[i] / 10), BINS - 1);
                hist[bin]++;
            }
            return hist;
        },
        [](vector<int> a, vector<int> b) -> vector<int> {
            for(size_t i = 0; i < a.size(); ++i)
                a[i] += b[i];
            return a;
        }
    );
    
    cout << "Histogram (10 bins, 0-100):" << endl;
    for(int i = 0; i < BINS; ++i) {
        cout << "[" << i*10 << "-" << (i+1)*10 << "): "
             << histogram[i] << " ";
        // Print bar
        int bar_len = histogram[i] / (N / 100);
        for(int j = 0; j < bar_len; ++j) cout << "█";
        cout << endl;
    }
}

// ============= Main =============

int main() {
    cout << "╔════════════════════════════════════════════════════════╗" << endl;
    cout << "║      TBB parallel_reduce - Complete Tutorial           ║" << endl;
    cout << "╚════════════════════════════════════════════════════════╝" << endl;
    
    example1_simple_sum();
    example2_minimum();
    example3_dot_product();
    example4_custom_find();
    example5_statistics();
    example6_deterministic();
    example7_histogram();
    
    cout << "\n";
    cout << "╔════════════════════════════════════════════════════════╗" << endl;
    cout << "║                    KEY TAKEAWAYS                       ║" << endl;
    cout << "╠════════════════════════════════════════════════════════╣" << endl;
    cout << "║                                                        ║" << endl;
    cout << "║  1. parallel_reduce is for aggregation operations      ║" << endl;
    cout << "║  2. Requires identity value and join operation         ║" << endl;
    cout << "║  3. Operation must be associative                      ║" << endl;
    cout << "║  4. Great for sum, product, min, max, custom ops       ║" << endl;
    cout << "║  5. Can return complex types (structs, vectors)        ║" << endl;
    cout << "║  6. Use deterministic version for reproducibility      ║" << endl;
    cout << "║  7. Much faster than sequential for large data         ║" << endl;
    cout << "║                                                        ║" << endl;
    cout << "╚════════════════════════════════════════════════════════╝" << endl;
    
    return 0;
}

