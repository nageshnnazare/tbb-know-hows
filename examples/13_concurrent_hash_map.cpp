/*
 * TBB CONCURRENT_HASH_MAP - Thread-Safe Hash Table
 * 
 * DEFINITION:
 * concurrent_hash_map is a thread-safe hash table that allows
 * concurrent insertions, lookups, and deletions with fine-grained locking.
 * 
 * KEY FEATURES:
 * - Fine-grained locking (per-bucket)
 * - Concurrent insert, find, erase
 * - Accessor pattern for safe read/write
 * - Never invalidates iterators
 * - Automatic resizing
 * 
 * WHEN TO USE:
 * - Concurrent key-value storage
 * - Caches and lookup tables
 * - Shared dictionaries
 * - Symbol tables
 * - Configuration maps
 * 
 * PERFORMANCE:
 * - Better than mutex + std::unordered_map
 * - Scales with thread count
 * - Lock-free reads possible with const_accessor
 */

#include <iostream>
#include <algorithm>
#include <cmath>
#include <string>
#include <vector>
#include <thread>
#include <chrono>
#include <tbb/concurrent_hash_map.h>
#include <tbb/parallel_for.h>
#include <tbb/blocked_range.h>

using namespace std;
using namespace tbb;
using namespace chrono;

// ============= Example 1: Basic Operations =============

void example1_basic() {
    cout << "\n=== Example 1: Basic Operations ===" << endl;
    
    concurrent_hash_map<string, int> map;
    
    // Insert
    map.insert(make_pair("apple", 5));
    map.insert(make_pair("banana", 3));
    map.insert(make_pair("cherry", 7));
    
    cout << "Inserted 3 items" << endl;
    
    // Find with accessor (read-write)
    {
        concurrent_hash_map<string, int>::accessor a;
        if(map.find(a, "apple")) {
            cout << "Found 'apple': " << a->second << endl;
            a->second = 10;  // Modify while locked
            cout << "Modified to: " << a->second << endl;
        }
    } // Lock released
    
    // Find with const_accessor (read-only)
    {
        concurrent_hash_map<string, int>::const_accessor ca;
        if(map.find(ca, "apple")) {
            cout << "Read 'apple': " << ca->second << endl;
            // ca->second = 20;  // ERROR: const accessor
        }
    }
    
    // Size and erase
    cout << "Map size: " << map.size() << endl;
    map.erase("banana");
    cout << "After erase: " << map.size() << endl;
}

// ============= Example 2: Concurrent Insertions =============

void example2_concurrent_insert() {
    cout << "\n=== Example 2: Concurrent Insertions ===" << endl;
    
    concurrent_hash_map<int, int> map;
    const int NUM_THREADS = 4;
    const int ITEMS_PER_THREAD = 1000;
    
    auto start = high_resolution_clock::now();
    
    // Multiple threads inserting
    vector<thread> threads;
    for(int t = 0; t < NUM_THREADS; ++t) {
        threads.emplace_back([&, t]() {
            for(int i = 0; i < ITEMS_PER_THREAD; ++i) {
                int key = t * ITEMS_PER_THREAD + i;
                map.insert(make_pair(key, key * key));
            }
        });
    }
    
    for(auto& th : threads) th.join();
    
    auto elapsed = duration_cast<milliseconds>(
        high_resolution_clock::now() - start).count();
    
    cout << "Inserted " << map.size() << " items" << endl;
    cout << "Time: " << elapsed << " ms" << endl;
    cout << "Expected: " << NUM_THREADS * ITEMS_PER_THREAD << endl;
}

// ============= Example 3: Accessor Pattern =============

struct UserData {
    string name;
    int age;
    double balance;
    
    UserData() : name(""), age(0), balance(0.0) {}
    UserData(string n, int a, double b) : name(n), age(a), balance(b) {}
};

void example3_accessor_pattern() {
    cout << "\n=== Example 3: Accessor Pattern ===" << endl;
    
    concurrent_hash_map<int, UserData> users;
    
    // Insert users
    users.insert(make_pair(1, UserData("Alice", 30, 1000.0)));
    users.insert(make_pair(2, UserData("Bob", 25, 1500.0)));
    users.insert(make_pair(3, UserData("Charlie", 35, 2000.0)));
    
    // Update user balance (thread-safe)
    {
        concurrent_hash_map<int, UserData>::accessor a;
        if(users.find(a, 1)) {
            cout << "User 1 (before): " << a->second.name 
                 << ", balance=" << a->second.balance << endl;
            a->second.balance += 500.0;  // Thread-safe update
            cout << "User 1 (after): " << a->second.name 
                 << ", balance=" << a->second.balance << endl;
        }
    } // Automatic lock release
    
    // Read-only access (allows concurrent reads)
    {
        concurrent_hash_map<int, UserData>::const_accessor ca;
        if(users.find(ca, 1)) {
            cout << "Read user: " << ca->second.name 
                 << ", balance=" << ca->second.balance << endl;
        }
    }
}

// ============= Example 4: Insert or Update Pattern =============

void example4_insert_or_update() {
    cout << "\n=== Example 4: Insert or Update ===" << endl;
    
    concurrent_hash_map<string, int> word_count;
    
    vector<string> words = {
        "hello", "world", "hello", "tbb", "world", 
        "hello", "concurrent", "map", "tbb"
    };
    
    // Count words concurrently
    parallel_for(blocked_range<size_t>(0, words.size()),
        [&](blocked_range<size_t> r) {
            for(size_t i = r.begin(); i != r.end(); ++i) {
                concurrent_hash_map<string, int>::accessor a;
                if(word_count.insert(a, words[i])) {
                    // New insertion
                    a->second = 1;
                } else {
                    // Already exists, increment
                    a->second++;
                }
            }
        });
    
    // Print results
    cout << "Word counts:" << endl;
    for(auto it = word_count.begin(); it != word_count.end(); ++it) {
        cout << "  " << it->first << ": " << it->second << endl;
    }
}

// ============= Example 5: Concurrent Cache Implementation =============

class ConcurrentCache {
    concurrent_hash_map<int, string> cache;
    tbb::atomic<int> hits;
    tbb::atomic<int> misses;
    
public:
    ConcurrentCache() {
        hits = 0;
        misses = 0;
    }
    
    string get(int key) {
        concurrent_hash_map<int, string>::const_accessor ca;
        if(cache.find(ca, key)) {
            hits++;
            return ca->second;
        }
        misses++;
        return "";
    }
    
    void put(int key, const string& value) {
        concurrent_hash_map<int, string>::accessor a;
        if(cache.insert(a, key)) {
            a->second = value;
        } else {
            a->second = value;  // Update existing
        }
    }
    
    void stats() {
        cout << "Cache size: " << cache.size() << endl;
        cout << "Hits: " << hits << ", Misses: " << misses << endl;
        cout << "Hit rate: " << (100.0 * hits / (hits + misses)) << "%" << endl;
    }
};

void example5_cache() {
    cout << "\n=== Example 5: Concurrent Cache ===" << endl;
    
    ConcurrentCache cache;
    
    // Populate cache
    for(int i = 0; i < 100; ++i) {
        cache.put(i, "Value_" + to_string(i));
    }
    
    // Concurrent access
    parallel_for(blocked_range<int>(0, 1000),
        [&](blocked_range<int> r) {
            for(int i = r.begin(); i != r.end(); ++i) {
                int key = rand() % 150;  // Some hits, some misses
                string val = cache.get(key);
            }
        });
    
    cache.stats();
}

// ============= Example 6: Concurrent Updates =============

void example6_concurrent_updates() {
    cout << "\n=== Example 6: Concurrent Updates ===" << endl;
    
    concurrent_hash_map<int, tbb::atomic<int>> counters;
    
    // Initialize counters
    for(int i = 0; i < 10; ++i) {
        concurrent_hash_map<int, tbb::atomic<int>>::accessor a;
        counters.insert(a, i);
        a->second = 0;
    }
    
    // Multiple threads incrementing
    parallel_for(blocked_range<int>(0, 10000),
        [&](blocked_range<int> r) {
            for(int i = r.begin(); i != r.end(); ++i) {
                int counter_id = i % 10;
                concurrent_hash_map<int, tbb::atomic<int>>::accessor a;
                if(counters.find(a, counter_id)) {
                    a->second++;
                }
            }
        });
    
    // Print results
    cout << "Counter values:" << endl;
    for(auto it = counters.begin(); it != counters.end(); ++it) {
        cout << "  Counter " << it->first << ": " << it->second << endl;
    }
}

// ============= Example 7: Performance vs std::unordered_map =============

void example7_performance() {
    cout << "\n=== Example 7: Performance Comparison ===" << endl;
    
    const int N = 100000;
    concurrent_hash_map<int, int> tbb_map;
    
    // TBB concurrent_hash_map with parallel insertions
    auto start = high_resolution_clock::now();
    
    parallel_for(blocked_range<int>(0, N),
        [&](blocked_range<int> r) {
            for(int i = r.begin(); i != r.end(); ++i) {
                tbb_map.insert(make_pair(i, i * i));
            }
        });
    
    auto tbb_time = duration_cast<milliseconds>(
        high_resolution_clock::now() - start).count();
    
    cout << "TBB concurrent_hash_map (parallel): " << tbb_time << " ms" << endl;
    cout << "Inserted " << tbb_map.size() << " items" << endl;
    cout << "Fine-grained locking enables true concurrency" << endl;
}

int main() {
    cout << "╔════════════════════════════════════════════════════════╗" << endl;
    cout << "║  TBB concurrent_hash_map - Complete Tutorial          ║" << endl;
    cout << "╚════════════════════════════════════════════════════════╝" << endl;
    
    example1_basic();
    example2_concurrent_insert();
    example3_accessor_pattern();
    example4_insert_or_update();
    example5_cache();
    example6_concurrent_updates();
    example7_performance();
    
    cout << "\n╔════════════════════════════════════════════════════════╗" << endl;
    cout << "║                    KEY TAKEAWAYS                       ║" << endl;
    cout << "╠════════════════════════════════════════════════════════╣" << endl;
    cout << "║                                                        ║" << endl;
    cout << "║  1. Fine-grained locking per bucket                   ║" << endl;
    cout << "║  2. accessor locks for read-write access              ║" << endl;
    cout << "║  3. const_accessor for read-only (allows concurrent)  ║" << endl;
    cout << "║  4. insert(accessor, key) for atomic insert/find      ║" << endl;
    cout << "║  5. Never invalidates iterators                       ║" << endl;
    cout << "║  6. Automatic resizing without global lock            ║" << endl;
    cout << "║  7. Perfect for concurrent caches and lookups         ║" << endl;
    cout << "║                                                        ║" << endl;
    cout << "╠════════════════════════════════════════════════════════╣" << endl;
    cout << "║               COMMON PATTERNS                          ║" << endl;
    cout << "╠════════════════════════════════════════════════════════╣" << endl;
    cout << "║                                                        ║" << endl;
    cout << "║  Insert:                                               ║" << endl;
    cout << "║    map.insert(make_pair(key, value));                 ║" << endl;
    cout << "║                                                        ║" << endl;
    cout << "║  Find and Modify:                                      ║" << endl;
    cout << "║    concurrent_hash_map<K,V>::accessor a;              ║" << endl;
    cout << "║    if(map.find(a, key)) {                             ║" << endl;
    cout << "║        a->second = new_value;  // Locked              ║" << endl;
    cout << "║    }                                                   ║" << endl;
    cout << "║                                                        ║" << endl;
    cout << "║  Insert or Update:                                     ║" << endl;
    cout << "║    concurrent_hash_map<K,V>::accessor a;              ║" << endl;
    cout << "║    if(map.insert(a, key)) {                           ║" << endl;
    cout << "║        a->second = initial_value;  // New             ║" << endl;
    cout << "║    } else {                                            ║" << endl;
    cout << "║        a->second += delta;  // Update existing        ║" << endl;
    cout << "║    }                                                   ║" << endl;
    cout << "║                                                        ║" << endl;
    cout << "╠════════════════════════════════════════════════════════╣" << endl;
    cout << "║               WHEN TO USE                              ║" << endl;
    cout << "╠════════════════════════════════════════════════════════╣" << endl;
    cout << "║                                                        ║" << endl;
    cout << "║  ✓ Concurrent key-value storage                       ║" << endl;
    cout << "║  ✓ Shared caches and lookup tables                    ║" << endl;
    cout << "║  ✓ Symbol tables in compilers                         ║" << endl;
    cout << "║  ✓ Configuration dictionaries                         ║" << endl;
    cout << "║  ✓ Session management                                 ║" << endl;
    cout << "║  ✗ Simple cases (use concurrent_unordered_map)        ║" << endl;
    cout << "║  ✗ Need range queries (use different structure)       ║" << endl;
    cout << "║                                                        ║" << endl;
    cout << "╚════════════════════════════════════════════════════════╝" << endl;
    
    return 0;
}
