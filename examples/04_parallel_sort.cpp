/*
 * TBB PARALLEL_SORT - Parallel Sorting Algorithms
 * 
 * DEFINITION:
 * parallel_sort sorts a range using a parallel quicksort algorithm
 * with automatic grain size selection.
 * 
 * FEATURES:
 * - Parallel quicksort implementation
 * - Custom comparators supported
 * - In-place sorting
 * - Automatic task granularity
 * - O(N log N) average case
 */

#include <iostream>
#include <cmath>
#include <vector>
#include <algorithm>
#include <tbb/parallel_sort.h>
#include <chrono>
#include <random>
#include <string>

using namespace std;
using namespace tbb;
using namespace chrono;

// ============= Example 1: Basic Sorting =============

void example1_basic_sort() {
    cout << "\n=== Example 1: Basic parallel_sort ===" << endl;
    
    vector<int> data = {9, 3, 7, 1, 5, 2, 8, 4, 6};
    
    cout << "Before: ";
    for(int x : data) cout << x << " ";
    cout << endl;
    
    parallel_sort(data.begin(), data.end());
    
    cout << "After:  ";
    for(int x : data) cout << x << " ";
    cout << endl;
    
    bool sorted = is_sorted(data.begin(), data.end());
    cout << "Verification: " << (sorted ? "✓ SORTED" : "✗ NOT SORTED") << endl;
}

// ============= Example 2: Custom Comparator =============

struct Person {
    string name;
    int age;
    
    Person(string n, int a) : name(n), age(a) {}
};

void example2_custom_comparator() {
    cout << "\n=== Example 2: Custom Comparator ===" << endl;
    
    vector<Person> people = {
        {"Alice", 30},
        {"Bob", 25},
        {"Charlie", 35},
        {"David", 28}
    };
    
    // Sort by age
    parallel_sort(people.begin(), people.end(),
        [](const Person& a, const Person& b) {
            return a.age < b.age;
        });
    
    cout << "Sorted by age:" << endl;
    for(const auto& p : people)
        cout << "  " << p.name << ": " << p.age << endl;
    
    // Sort by name
    parallel_sort(people.begin(), people.end(),
        [](const Person& a, const Person& b) {
            return a.name < b.name;
        });
    
    cout << "Sorted by name:" << endl;
    for(const auto& p : people)
        cout << "  " << p.name << ": " << p.age << endl;
}

// ============= Example 3: Performance Comparison =============

void example3_performance() {
    cout << "\n=== Example 3: Performance Comparison ===" << endl;
    
    const int N = 10000000;
    random_device rd;
    mt19937 gen(rd());
    uniform_int_distribution<> dis(1, 1000000);
    
    // Generate random data
    vector<int> data(N);
    for(int& x : data) x = dis(gen);
    
    // std::sort (sequential)
    vector<int> data_seq = data;
    auto start = high_resolution_clock::now();
    sort(data_seq.begin(), data_seq.end());
    auto seq_time = duration_cast<milliseconds>(
        high_resolution_clock::now() - start).count();
    
    // parallel_sort
    vector<int> data_par = data;
    start = high_resolution_clock::now();
    parallel_sort(data_par.begin(), data_par.end());
    auto par_time = duration_cast<milliseconds>(
        high_resolution_clock::now() - start).count();
    
    cout << "Sorting " << N << " random integers:" << endl;
    cout << "std::sort:      " << seq_time << " ms" << endl;
    cout << "parallel_sort:  " << par_time << " ms" << endl;
    cout << "Speedup:        " << (double)seq_time / par_time << "x" << endl;
    
    bool same = (data_seq == data_par);
    cout << "Results match:  " << (same ? "✓" : "✗") << endl;
}

// ============= Example 4: Descending Order =============

void example4_descending() {
    cout << "\n=== Example 4: Descending Order ===" << endl;
    
    vector<int> data = {3, 1, 4, 1, 5, 9, 2, 6, 5, 3};
    
    cout << "Original: ";
    for(int x : data) cout << x << " ";
    cout << endl;
    
    parallel_sort(data.begin(), data.end(), greater<int>());
    
    cout << "Descending: ";
    for(int x : data) cout << x << " ";
    cout << endl;
}

// ============= Example 5: Sorting Different Types =============

void example5_different_types() {
    cout << "\n=== Example 5: Sorting Different Types ===" << endl;
    
    // Doubles
    vector<double> doubles = {3.14, 2.71, 1.41, 1.73, 2.23};
    parallel_sort(doubles.begin(), doubles.end());
    cout << "Doubles: ";
    for(double x : doubles) cout << x << " ";
    cout << endl;
    
    // Strings
    vector<string> strings = {"zebra", "apple", "mango", "banana", "cherry"};
    parallel_sort(strings.begin(), strings.end());
    cout << "Strings: ";
    for(const string& s : strings) cout << s << " ";
    cout << endl;
    
    // Pairs
    vector<pair<int,int>> pairs = {{3,1}, {1,4}, {1,5}, {2,6}};
    parallel_sort(pairs.begin(), pairs.end());
    cout << "Pairs: ";
    for(auto [a,b] : pairs) cout << "(" << a << "," << b << ") ";
    cout << endl;
}

// ============= Example 6: Partial Sort Use Case =============

void example6_top_k() {
    cout << "\n=== Example 6: Finding Top K Elements ===" << endl;
    
    const int N = 1000000;
    const int K = 10;
    
    random_device rd;
    mt19937 gen(rd());
    uniform_int_distribution<> dis(1, 1000000);
    
    vector<int> data(N);
    for(int& x : data) x = dis(gen);
    
    // Sort and get top K
    auto start = high_resolution_clock::now();
    parallel_sort(data.begin(), data.end(), greater<int>());
    auto elapsed = duration_cast<milliseconds>(
        high_resolution_clock::now() - start).count();
    
    cout << "Sorted " << N << " elements in " << elapsed << " ms" << endl;
    cout << "Top " << K << " elements: ";
    for(int i = 0; i < K; ++i)
        cout << data[i] << " ";
    cout << endl;
}

// ============= Example 7: Stability Test =============

void example7_stability() {
    cout << "\n=== Example 7: Stability Note ===" << endl;
    
    struct Item {
        int key;
        int order;
        Item(int k, int o) : key(k), order(o) {}
    };
    
    vector<Item> items;
    items.push_back(Item(1, 1));
    items.push_back(Item(2, 2));
    items.push_back(Item(1, 3));
    items.push_back(Item(2, 4));
    items.push_back(Item(1, 5));
    
    parallel_sort(items.begin(), items.end(),
        [](const Item& a, const Item& b) {
            return a.key < b.key;
        });
    
    cout << "Note: parallel_sort is NOT stable" << endl;
    cout << "After sorting by key:" << endl;
    for(const auto& item : items)
        cout << "  key=" << item.key << ", order=" << item.order << endl;
    
    cout << "\nFor stable sort, use tbb::parallel_stable_sort" << endl;
    cout << "(if available in your TBB version)" << endl;
}

int main() {
    cout << "╔════════════════════════════════════════════════════════╗" << endl;
    cout << "║      TBB parallel_sort - Complete Tutorial            ║" << endl;
    cout << "╚════════════════════════════════════════════════════════╝" << endl;
    
    example1_basic_sort();
    example2_custom_comparator();
    example3_performance();
    example4_descending();
    example5_different_types();
    example6_top_k();
    example7_stability();
    
    cout << "\n╔════════════════════════════════════════════════════════╗" << endl;
    cout << "║                    KEY TAKEAWAYS                       ║" << endl;
    cout << "╠════════════════════════════════════════════════════════╣" << endl;
    cout << "║                                                        ║" << endl;
    cout << "║  1. parallel_sort is faster than std::sort for large  ║" << endl;
    cout << "║     datasets (typically 2-4x speedup)                 ║" << endl;
    cout << "║  2. Supports custom comparators like std::sort        ║" << endl;
    cout << "║  3. In-place sorting (no extra memory)                ║" << endl;
    cout << "║  4. NOT stable (equal elements may reorder)           ║" << endl;
    cout << "║  5. Works with any comparable type                    ║" << endl;
    cout << "║  6. Automatic parallelism (no tuning needed)          ║" << endl;
    cout << "║                                                        ║" << endl;
    cout << "╚════════════════════════════════════════════════════════╝" << endl;
    
    return 0;
}

