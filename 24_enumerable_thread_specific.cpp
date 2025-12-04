/*
 * TBB ENUMERABLE_THREAD_SPECIFIC - Thread-Local Storage
 * 
 * DEFINITION:
 * enumerable_thread_specific (ETS) provides thread-local storage where
 * each thread gets its own copy of data, eliminating synchronization.
 * 
 * KEY FEATURES:
 * - Thread-local data (no locking needed)
 * - Enumerable (can iterate over all copies)
 * - Automatic initialization per thread
 * - Combining support
 * - Cache-line padding
 * 
 * WHEN TO USE:
 * - Thread-local accumulation
 * - Eliminate contention
 * - Per-thread buffers
 * - Histograms and counting
 * - Reduce synchronization
 * 
 * BENEFITS:
 * + Zero synchronization overhead
 * + Perfect scalability
 * + Cache-friendly
 * + Easy to combine results
 */

#include <iostream>
#include <cmath>
#include <vector>
#include <string>
#include <chrono>
#include <tbb/enumerable_thread_specific.h>
#include <tbb/parallel_for.h>
#include <tbb/blocked_range.h>

using namespace std;
using namespace tbb;
using namespace chrono;

// ============= Example 1: Basic Thread-Local Accumulation =============

void example1_accumulation() {
    cout << "\n=== Example 1: Thread-Local Accumulation ===" << endl;
    
    const int N = 10000000;
    vector<int> data(N, 1);
    
    // Each thread gets its own sum
    enumerable_thread_specific<int> thread_local_sum(0);
    
    auto start = high_resolution_clock::now();
    
    parallel_for(blocked_range<int>(0, N),
        [&](blocked_range<int> r) {
            int& my_sum = thread_local_sum.local();  // Get thread-local copy
            for(int i = r.begin(); i != r.end(); ++i) {
                my_sum += data[i];  // No synchronization needed!
            }
        });
    
    // Combine all thread-local sums
    int total = 0;
    for(auto& local_sum : thread_local_sum) {
        total += local_sum;
    }
    
    auto elapsed = duration_cast<milliseconds>(
        high_resolution_clock::now() - start).count();
    
    cout << "Total sum: " << total << endl;
    cout << "Time: " << elapsed << " ms" << endl;
    cout << "Number of threads: " << distance(thread_local_sum.begin(), 
                                              thread_local_sum.end()) << endl;
}

// ============= Example 2: Performance vs Atomic =============

void example2_performance() {
    cout << "\n=== Example 2: Performance vs Atomic ===" << endl;
    
    const int N = 10000000;
    vector<int> data(N, 1);
    
    // With atomic (synchronization overhead)
    tbb::atomic<long long> atomic_sum;
    atomic_sum = 0;
    
    auto start = high_resolution_clock::now();
    parallel_for(blocked_range<int>(0, N),
        [&](blocked_range<int> r) {
            for(int i = r.begin(); i != r.end(); ++i) {
                atomic_sum += data[i];  // Atomic operation
            }
        });
    auto atomic_time = duration_cast<milliseconds>(
        high_resolution_clock::now() - start).count();
    
    // With ETS (no synchronization)
    enumerable_thread_specific<long long> ets_sum(0);
    
    start = high_resolution_clock::now();
    parallel_for(blocked_range<int>(0, N),
        [&](blocked_range<int> r) {
            long long& my_sum = ets_sum.local();
            for(int i = r.begin(); i != r.end(); ++i) {
                my_sum += data[i];  // No synchronization!
            }
        });
    
    long long total = 0;
    for(auto& s : ets_sum) total += s;
    
    auto ets_time = duration_cast<milliseconds>(
        high_resolution_clock::now() - start).count();
    
    cout << "Atomic:   " << atomic_time << " ms" << endl;
    cout << "ETS:      " << ets_time << " ms" << endl;
    cout << "Speedup:  " << (double)atomic_time / ets_time << "x" << endl;
    cout << "Both results: " << atomic_sum << " vs " << total << endl;
}

// ============= Example 3: Per-Thread Histograms =============

void example3_histogram() {
    cout << "\n=== Example 3: Per-Thread Histograms ===" << endl;
    
    const int N = 1000000;
    const int BINS = 10;
    vector<int> data(N);
    
    // Generate random data
    for(int i = 0; i < N; ++i) {
        data[i] = rand() % 100;  // 0-99
    }
    
    // Each thread gets its own histogram
    enumerable_thread_specific<vector<int>> thread_histograms(
        []() { return vector<int>(BINS, 0); });  // Initializer
    
    parallel_for(blocked_range<int>(0, N),
        [&](blocked_range<int> r) {
            vector<int>& my_hist = thread_histograms.local();
            for(int i = r.begin(); i != r.end(); ++i) {
                int bin = data[i] / 10;  // 0-9
                my_hist[bin]++;
            }
        });
    
    // Combine histograms
    vector<int> final_histogram(BINS, 0);
    for(auto& hist : thread_histograms) {
        for(int i = 0; i < BINS; ++i) {
            final_histogram[i] += hist[i];
        }
    }
    
    // Display
    cout << "Histogram:" << endl;
    for(int i = 0; i < BINS; ++i) {
        cout << "[" << i*10 << "-" << (i+1)*10 << "): " 
             << final_histogram[i] << endl;
    }
}

// ============= Example 4: Thread-Local Buffers =============

void example4_buffers() {
    cout << "\n=== Example 4: Thread-Local Buffers ===" << endl;
    
    const int N = 1000;
    
    // Each thread gets its own buffer
    enumerable_thread_specific<vector<string>> thread_buffers;
    
    parallel_for(blocked_range<int>(0, N),
        [&](blocked_range<int> r) {
            vector<string>& my_buffer = thread_buffers.local();
            for(int i = r.begin(); i != r.end(); ++i) {
                my_buffer.push_back("Item_" + to_string(i));
            }
        });
    
    // Collect all buffers
    int total_items = 0;
    for(auto& buffer : thread_buffers) {
        total_items += buffer.size();
    }
    
    cout << "Total items collected: " << total_items << endl;
    cout << "Number of threads: " 
         << distance(thread_buffers.begin(), thread_buffers.end()) << endl;
    cout << "Each thread had its own buffer - no contention!" << endl;
}

// ============= Example 5: Custom Initialization =============

struct ThreadData {
    int thread_id;
    long long sum;
    int count;
    
    ThreadData() : thread_id(0), sum(0), count(0) {}
};

void example5_custom_init() {
    cout << "\n=== Example 5: Custom Initialization ===" << endl;
    
    tbb::atomic<int> next_thread_id;
    next_thread_id = 0;
    
    // Custom initializer for each thread
    enumerable_thread_specific<ThreadData> thread_data(
        [&]() {
            ThreadData td;
            td.thread_id = next_thread_id.fetch_and_increment();
            return td;
        });
    
    const int N = 100000;
    vector<int> data(N);
    for(int i = 0; i < N; ++i) data[i] = i;
    
    parallel_for(blocked_range<int>(0, N),
        [&](blocked_range<int> r) {
            ThreadData& my_data = thread_data.local();
            for(int i = r.begin(); i != r.end(); ++i) {
                my_data.sum += data[i];
                my_data.count++;
            }
        });
    
    // Display per-thread stats
    cout << "Per-thread statistics:" << endl;
    long long total_sum = 0;
    int total_count = 0;
    
    for(auto& td : thread_data) {
        cout << "  Thread " << td.thread_id << ": processed " 
             << td.count << " items, sum=" << td.sum << endl;
        total_sum += td.sum;
        total_count += td.count;
    }
    
    cout << "Total: " << total_count << " items, sum=" << total_sum << endl;
}

// ============= Example 6: Combining with combine() =============

void example6_combine() {
    cout << "\n=== Example 6: Using combine() ===" << endl;
    
    const int N = 1000000;
    vector<double> data(N);
    for(int i = 0; i < N; ++i) data[i] = i * 0.001;
    
    enumerable_thread_specific<double> thread_sums(0.0);
    
    parallel_for(blocked_range<int>(0, N),
        [&](blocked_range<int> r) {
            double& my_sum = thread_sums.local();
            for(int i = r.begin(); i != r.end(); ++i) {
                my_sum += data[i];
            }
        });
    
    // Combine using functor
    double total = thread_sums.combine(
        [](double a, double b) { return a + b; });
    
    cout << "Combined sum: " << total << endl;
    cout << "combine() merges all thread-local values" << endl;
}

// ============= Example 7: Real-World Example - Text Processing =============

struct WordStats {
    int word_count;
    int char_count;
    int line_count;
    
    WordStats() : word_count(0), char_count(0), line_count(0) {}
    
    void merge(const WordStats& other) {
        word_count += other.word_count;
        char_count += other.char_count;
        line_count += other.line_count;
    }
};

void example7_text_processing() {
    cout << "\n=== Example 7: Text Processing ===" << endl;
    
    vector<string> lines = {
        "Hello world", "TBB is great", "Parallel programming",
        "Thread local storage", "No synchronization needed",
        "Fast and scalable", "Cache friendly design",
        "Enumerable thread specific", "Combine results easily"
    };
    
    enumerable_thread_specific<WordStats> thread_stats;
    
    parallel_for(blocked_range<size_t>(0, lines.size()),
        [&](blocked_range<size_t> r) {
            WordStats& my_stats = thread_stats.local();
            for(size_t i = r.begin(); i != r.end(); ++i) {
                my_stats.line_count++;
                my_stats.char_count += lines[i].length();
                
                // Count words (simple: count spaces + 1)
                int words = 1;
                for(char c : lines[i]) {
                    if(c == ' ') words++;
                }
                my_stats.word_count += words;
            }
        });
    
    // Combine all thread stats
    WordStats total;
    for(auto& stats : thread_stats) {
        total.merge(stats);
    }
    
    cout << "Text statistics:" << endl;
    cout << "  Lines: " << total.line_count << endl;
    cout << "  Words: " << total.word_count << endl;
    cout << "  Chars: " << total.char_count << endl;
    cout << "Each thread processed its subset independently!" << endl;
}

int main() {
    cout << "╔════════════════════════════════════════════════════════╗" << endl;
    cout << "║  TBB enumerable_thread_specific - Complete Tutorial   ║" << endl;
    cout << "╚════════════════════════════════════════════════════════╝" << endl;
    
    example1_accumulation();
    example2_performance();
    example3_histogram();
    example4_buffers();
    example5_custom_init();
    example6_combine();
    example7_text_processing();
    
    cout << "\n╔════════════════════════════════════════════════════════╗" << endl;
    cout << "║                    KEY TAKEAWAYS                       ║" << endl;
    cout << "╠════════════════════════════════════════════════════════╣" << endl;
    cout << "║                                                        ║" << endl;
    cout << "║  1. Each thread gets its own copy of data             ║" << endl;
    cout << "║  2. Zero synchronization overhead                     ║" << endl;
    cout << "║  3. local() returns thread-local reference            ║" << endl;
    cout << "║  4. Can iterate over all thread-local copies          ║" << endl;
    cout << "║  5. combine() merges all copies with functor          ║" << endl;
    cout << "║  6. Much faster than atomics for accumulation         ║" << endl;
    cout << "║  7. Perfect for histograms, buffers, counters         ║" << endl;
    cout << "║  8. Cache-friendly (no false sharing)                 ║" << endl;
    cout << "║                                                        ║" << endl;
    cout << "╠════════════════════════════════════════════════════════╣" << endl;
    cout << "║               COMMON PATTERNS                          ║" << endl;
    cout << "╠════════════════════════════════════════════════════════╣" << endl;
    cout << "║                                                        ║" << endl;
    cout << "║  Simple Sum:                                           ║" << endl;
    cout << "║    enumerable_thread_specific<int> sums(0);           ║" << endl;
    cout << "║    parallel_for(..., [&](...) {                       ║" << endl;
    cout << "║        sums.local() += value;                         ║" << endl;
    cout << "║    });                                                 ║" << endl;
    cout << "║    int total = sums.combine(plus<int>());             ║" << endl;
    cout << "║                                                        ║" << endl;
    cout << "║  With Initializer:                                     ║" << endl;
    cout << "║    enumerable_thread_specific<vector<int>> buffers(   ║" << endl;
    cout << "║        []() { return vector<int>(); });               ║" << endl;
    cout << "║                                                        ║" << endl;
    cout << "║  Iterate All:                                          ║" << endl;
    cout << "║    for(auto& local : ets) {                           ║" << endl;
    cout << "║        process(local);                                 ║" << endl;
    cout << "║    }                                                   ║" << endl;
    cout << "║                                                        ║" << endl;
    cout << "╠════════════════════════════════════════════════════════╣" << endl;
    cout << "║               WHEN TO USE                              ║" << endl;
    cout << "╠════════════════════════════════════════════════════════╣" << endl;
    cout << "║                                                        ║" << endl;
    cout << "║  ✓ Thread-local accumulation                          ║" << endl;
    cout << "║  ✓ Per-thread buffers or caches                       ║" << endl;
    cout << "║  ✓ Histograms and statistics                          ║" << endl;
    cout << "║  ✓ Eliminate atomic contention                        ║" << endl;
    cout << "║  ✓ Reduce synchronization                             ║" << endl;
    cout << "║  ✓ Performance-critical reductions                    ║" << endl;
    cout << "║  ✗ Need immediate global visibility (use atomic)      ║" << endl;
    cout << "║  ✗ Single global counter needed (use atomic)          ║" << endl;
    cout << "║                                                        ║" << endl;
    cout << "╚════════════════════════════════════════════════════════╝" << endl;
    
    return 0;
}
