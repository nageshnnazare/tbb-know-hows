/*
 * TBB PARALLEL_SCAN - Parallel Prefix Sum (Cumulative Sum)
 * 
 * DEFINITION:
 * parallel_scan computes a prefix operation in parallel, where each
 * output element is the reduction of all preceding input elements.
 * 
 * COMMON USE CASES:
 * - Prefix sum (cumulative sum)
 * - Parallel prefix algorithms
 * - Stream compaction
 * - Load balancing calculations
 * 
 * HOW IT WORKS:
 * 1. Split range into chunks
 * 2. Compute local prefix sums
 * 3. Combine partial results
 * 4. Update with global prefix
 */

#include <iostream>
#include <cmath>
#include <vector>
#include <tbb/parallel_scan.h>
#include <tbb/blocked_range.h>
#include <numeric>
#include <chrono>

using namespace std;
using namespace tbb;
using namespace chrono;

// ============= Example 1: Basic Prefix Sum =============

class PrefixSum {
    const vector<int>& input;
    vector<int>& output;
    int sum;
    
public:
    PrefixSum(const vector<int>& in, vector<int>& out) 
        : input(in), output(out), sum(0) {}
    
    PrefixSum(PrefixSum& b, split) 
        : input(b.input), output(b.output), sum(0) {}
    
    template<typename Tag>
    void operator()(const blocked_range<size_t>& r, Tag) {
        int temp = sum;
        for(size_t i = r.begin(); i < r.end(); ++i) {
            temp += input[i];
            if(Tag::is_final_scan())
                output[i] = temp;
        }
        sum = temp;
    }
    
    void reverse_join(PrefixSum& a) { sum = a.sum + sum; }
    void assign(PrefixSum& b) { sum = b.sum; }
};

void example1_basic_prefix_sum() {
    cout << "\n=== Example 1: Basic Prefix Sum ===" << endl;
    
    vector<int> input = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
    vector<int> output(input.size());
    
    PrefixSum body(input, output);
    parallel_scan(blocked_range<size_t>(0, input.size()), body);
    
    cout << "Input:  ";
    for(int x : input) cout << x << " ";
    cout << "\nOutput: ";
    for(int x : output) cout << x << " ";
    cout << endl;
    
    // Verify
    vector<int> expected(input.size());
    partial_sum(input.begin(), input.end(), expected.begin());
    bool correct = (output == expected);
    cout << "Verification: " << (correct ? "✓ PASSED" : "✗ FAILED") << endl;
}

// ============= Example 2: Lambda-based Prefix Sum =============

void example2_lambda_prefix_sum() {
    cout << "\n=== Example 2: Lambda-based Prefix Sum ===" << endl;
    
    const int N = 1000000;
    vector<int> input(N);
    vector<int> output(N);
    
    for(int i = 0; i < N; ++i) input[i] = i + 1;
    
    auto start = high_resolution_clock::now();
    
    parallel_scan(
        blocked_range<size_t>(0, N),
        0,  // identity
        [&](const blocked_range<size_t>& r, int sum, bool is_final) {
            for(size_t i = r.begin(); i < r.end(); ++i) {
                sum += input[i];
                if(is_final) output[i] = sum;
            }
            return sum;
        },
        [](int a, int b) { return a + b; }
    );
    
    auto elapsed = duration_cast<milliseconds>(
        high_resolution_clock::now() - start).count();
    
    cout << "Computed prefix sum of " << N << " elements in " 
         << elapsed << " ms" << endl;
    cout << "Sample: output[999999] = " << output[999999] << endl;
    cout << "Expected: " << (long long)N * (N + 1) / 2 << endl;
}

// ============= Example 3: Running Maximum =============

void example3_running_maximum() {
    cout << "\n=== Example 3: Running Maximum ===" << endl;
    
    vector<int> input = {3, 1, 4, 1, 5, 9, 2, 6, 5, 3};
    vector<int> output(input.size());
    
    parallel_scan(
        blocked_range<size_t>(0, input.size()),
        numeric_limits<int>::min(),
        [&](const blocked_range<size_t>& r, int current_max, bool is_final) {
            for(size_t i = r.begin(); i < r.end(); ++i) {
                current_max = max(current_max, input[i]);
                if(is_final) output[i] = current_max;
            }
            return current_max;
        },
        [](int a, int b) { return max(a, b); }
    );
    
    cout << "Input:          ";
    for(int x : input) cout << x << " ";
    cout << "\nRunning Max:    ";
    for(int x : output) cout << x << " ";
    cout << endl;
}

// ============= Example 4: Parallel Counting (Stream Compaction) =============

void example4_stream_compaction() {
    cout << "\n=== Example 4: Stream Compaction (Count & Pack) ===" << endl;
    
    vector<int> input = {3, -1, 4, -2, 5, 9, -3, 6, 5, -4, 3};
    vector<int> indices(input.size());
    
    // Count positive numbers up to each position
    parallel_scan(
        blocked_range<size_t>(0, input.size()),
        0,
        [&](const blocked_range<size_t>& r, int count, bool is_final) {
            for(size_t i = r.begin(); i < r.end(); ++i) {
                if(input[i] > 0) count++;
                if(is_final) indices[i] = count;
            }
            return count;
        },
        plus<int>()
    );
    
    int total_positive = indices.back();
    vector<int> packed(total_positive);
    
    // Pack positive numbers
    for(size_t i = 0; i < input.size(); ++i) {
        if(input[i] > 0) {
            packed[indices[i] - 1] = input[i];
        }
    }
    
    cout << "Input:    ";
    for(int x : input) cout << x << " ";
    cout << "\nPositive: ";
    for(int x : packed) cout << x << " ";
    cout << "\nTotal positive: " << total_positive << endl;
}

// ============= Example 5: Performance Comparison =============

void example5_performance() {
    cout << "\n=== Example 5: Performance Comparison ===" << endl;
    
    const int N = 10000000;
    vector<int> input(N);
    vector<int> output_seq(N);
    vector<int> output_par(N);
    
    for(int i = 0; i < N; ++i) input[i] = 1;
    
    // Sequential
    auto start = high_resolution_clock::now();
    partial_sum(input.begin(), input.end(), output_seq.begin());
    auto seq_time = duration_cast<milliseconds>(
        high_resolution_clock::now() - start).count();
    
    // Parallel
    start = high_resolution_clock::now();
    parallel_scan(
        blocked_range<size_t>(0, N),
        0,
        [&](const blocked_range<size_t>& r, int sum, bool is_final) {
            for(size_t i = r.begin(); i < r.end(); ++i) {
                sum += input[i];
                if(is_final) output_par[i] = sum;
            }
            return sum;
        },
        plus<int>()
    );
    auto par_time = duration_cast<milliseconds>(
        high_resolution_clock::now() - start).count();
    
    cout << "Sequential: " << seq_time << " ms" << endl;
    cout << "Parallel:   " << par_time << " ms" << endl;
    cout << "Speedup:    " << (double)seq_time / par_time << "x" << endl;
    
    bool correct = (output_seq == output_par);
    cout << "Verification: " << (correct ? "✓ PASSED" : "✗ FAILED") << endl;
}

int main() {
    cout << "╔════════════════════════════════════════════════════════╗" << endl;
    cout << "║      TBB parallel_scan - Complete Tutorial            ║" << endl;
    cout << "╚════════════════════════════════════════════════════════╝" << endl;
    
    example1_basic_prefix_sum();
    example2_lambda_prefix_sum();
    example3_running_maximum();
    example4_stream_compaction();
    example5_performance();
    
    cout << "\n╔════════════════════════════════════════════════════════╗" << endl;
    cout << "║                    KEY TAKEAWAYS                       ║" << endl;
    cout << "╠════════════════════════════════════════════════════════╣" << endl;
    cout << "║                                                        ║" << endl;
    cout << "║  1. parallel_scan computes prefix operations           ║" << endl;
    cout << "║  2. Two-phase algorithm (prescan + final scan)         ║" << endl;
    cout << "║  3. is_final_scan flag indicates when to write output ║" << endl;
    cout << "║  4. Works with any associative operation               ║" << endl;
    cout << "║  5. Useful for cumulative sums, running max, etc.      ║" << endl;
    cout << "║  6. Can enable parallel stream compaction              ║" << endl;
    cout << "║                                                        ║" << endl;
    cout << "╚════════════════════════════════════════════════════════╝" << endl;
    
    return 0;
}

