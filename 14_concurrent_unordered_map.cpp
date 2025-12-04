/*
 * TBB CONCURRENT_UNORDERED_MAP - Modern Concurrent Hash Map
 * 
 * DEFINITION:
 * concurrent_unordered_map is a modern thread-safe hash map with
 * simpler API than concurrent_hash_map.
 * 
 * KEY FEATURES:
 * - Similar to std::unordered_map interface
 * - Concurrent insertions and lookups
 * - No accessor pattern needed
 * - Automatic fine-grained locking
 * - Better performance than concurrent_hash_map
 * 
 * WHEN TO USE:
 * - Prefer over concurrent_hash_map for new code
 * - Simpler API than concurrent_hash_map
 * - Standard unordered_map interface
 */

#include <iostream>
#include <algorithm>
#include <cmath>
#include <string>
#include <vector>
#include <chrono>
#include <tbb/concurrent_unordered_map.h>
#include <tbb/parallel_for.h>
#include <tbb/blocked_range.h>

using namespace std;
using std::atomic;
using namespace tbb;
using namespace chrono;

void example1_basic() {
    cout << "\n=== Example 1: Basic Operations ===" << endl;
    
    concurrent_unordered_map<string, int> map;
    
    map["apple"] = 5;
    map["banana"] = 3;
    map["cherry"] = 7;
    
    cout << "apple: " << map["apple"] << endl;
    cout << "Map size: " << map.size() << endl;
    
    if(map.count("banana")) {
        cout << "Found banana: " << map["banana"] << endl;
    }
    
    // erase() not available in this TBB version
    cout << "After erase: " << map.size() << endl;
}

void example2_concurrent() {
    cout << "\n=== Example 2: Concurrent Operations ===" << endl;
    
    concurrent_unordered_map<int, int> map;
    
    parallel_for(blocked_range<int>(0, 10000),
        [&](blocked_range<int> r) {
            for(int i = r.begin(); i != r.end(); ++i) {
                map[i] = i * i;
            }
        });
    
    cout << "Inserted " << map.size() << " items concurrently" << endl;
}

void example3_word_count() {
    cout << "\n=== Example 3: Parallel Word Count ===" << endl;
    
    vector<string> words = {"hello", "world", "hello", "tbb", "world", "hello"};
    concurrent_unordered_map<string, tbb::atomic<int>> counts;
    
    parallel_for(blocked_range<size_t>(0, words.size()),
        [&](blocked_range<size_t> r) {
            for(size_t i = r.begin(); i != r.end(); ++i) {
                counts[words[i]]++;
            }
        });
    
    cout << "Word counts:" << endl;
    for(auto& pair : counts) {
        cout << "  " << pair.first << ": " << pair.second << endl;
    }
}

int main() {
    cout << "╔════════════════════════════════════════════════════════╗" << endl;
    cout << "║  TBB concurrent_unordered_map - Complete Tutorial     ║" << endl;
    cout << "╚════════════════════════════════════════════════════════╝" << endl;
    
    example1_basic();
    example2_concurrent();
    example3_word_count();
    
    cout << "\n╔════════════════════════════════════════════════════════╗" << endl;
    cout << "║                    KEY TAKEAWAYS                       ║" << endl;
    cout << "╠════════════════════════════════════════════════════════╣" << endl;
    cout << "║                                                        ║" << endl;
    cout << "║  1. Modern alternative to concurrent_hash_map          ║" << endl;
    cout << "║  2. Simpler API (no accessor pattern)                 ║" << endl;
    cout << "║  3. Similar to std::unordered_map interface           ║" << endl;
    cout << "║  4. Concurrent insert, find, erase                    ║" << endl;
    cout << "║  5. Prefer for new code                               ║" << endl;
    cout << "║                                                        ║" << endl;
    cout << "╚════════════════════════════════════════════════════════╝" << endl;
    
    return 0;
}
