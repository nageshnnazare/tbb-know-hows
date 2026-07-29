/*
 * TBB READER-WRITER LOCKS - Multiple Readers, Single Writer
 * 
 * DEFINITION:
 * Reader-writer locks allow multiple threads to read concurrently,
 * but only one thread to write at a time. Readers and writers are mutually exclusive.
 * 
 * TBB PROVIDES:
 * - spin_rw_mutex: Spinlock-based RW lock
 * - queuing_rw_mutex: Fair queuing RW lock
 * - speculative_spin_rw_mutex: Hardware transactional memory
 * 
 * LOCK MODES:
 * - Read lock (false): Multiple threads can hold simultaneously
 * - Write lock (true): Exclusive access
 * 
 * WHEN TO USE:
 * - Read-heavy workloads
 * - Shared configuration data
 * - Caches with occasional updates
 * - Symbol tables
 * - Lookup tables
 * 
 * BENEFITS:
 * + Multiple concurrent readers
 * + Better than regular mutex for read-heavy loads
 * + Automatic reader/writer coordination
 */

#include <iostream>
#include <map>
#include <algorithm>
#include <cmath>
#include <vector>
#include <string>
#include <thread>
#include <chrono>
#include <tbb/spin_rw_mutex.h>
#include <tbb/queuing_rw_mutex.h>
#include <tbb/parallel_for.h>
#include <tbb/blocked_range.h>

using namespace std;
using std::atomic;
using namespace tbb;
using namespace chrono;

// ============= Example 1: Basic Reader-Writer Pattern =============

void example1_basic() {
    cout << "\n=== Example 1: Basic Reader-Writer Lock ===" << endl;
    
    spin_rw_mutex rw_mtx;
    int shared_data = 100;
    
    // Reader 1
    {
        spin_rw_mutex::scoped_lock read_lock(rw_mtx, false);  // false = read
        cout << "Reader 1: " << shared_data << endl;
    }
    
    // Reader 2 (can run concurrently with Reader 1)
    {
        spin_rw_mutex::scoped_lock read_lock(rw_mtx, false);  // read
        cout << "Reader 2: " << shared_data << endl;
    }
    
    // Writer (exclusive access)
    {
        spin_rw_mutex::scoped_lock write_lock(rw_mtx, true);  // true = write
        shared_data = 200;
        cout << "Writer: Updated to " << shared_data << endl;
    }
    
    // Reader 3 (sees updated value)
    {
        spin_rw_mutex::scoped_lock read_lock(rw_mtx, false);
        cout << "Reader 3: " << shared_data << endl;
    }
}

// ============= Example 2: Concurrent Reads =============

void example2_concurrent_reads() {
    cout << "\n=== Example 2: Concurrent Reads ===" << endl;
    
    spin_rw_mutex rw_mtx;
    vector<int> shared_config = {1, 2, 3, 4, 5};
    tbb::atomic<int> reader_count;
    reader_count = 0;
    
    auto start = high_resolution_clock::now();
    
    // Many concurrent readers
    parallel_for(blocked_range<int>(0, 1000),
        [&](blocked_range<int> r) {
            for(int i = r.begin(); i != r.end(); ++i) {
                spin_rw_mutex::scoped_lock read_lock(rw_mtx, false);
                // Multiple readers can hold lock simultaneously
                int sum = 0;
                for(int val : shared_config) {
                    sum += val;
                }
                reader_count++;
            }
        });
    
    auto elapsed = duration_cast<milliseconds>(
        high_resolution_clock::now() - start).count();
    
    cout << "Completed " << reader_count << " reads in " << elapsed << " ms" << endl;
    cout << "Multiple readers executed concurrently!" << endl;
}

// ============= Example 3: Mixed Reads and Writes =============

void example3_mixed() {
    cout << "\n=== Example 3: Mixed Reads and Writes ===" << endl;
    
    spin_rw_mutex rw_mtx;
    int shared_counter = 0;
    tbb::atomic<int> reads, writes;
    reads = 0;
    writes = 0;
    
    const int N = 1000;
    
    auto start = high_resolution_clock::now();
    
    parallel_for(blocked_range<int>(0, N),
        [&](blocked_range<int> r) {
            for(int i = r.begin(); i != r.end(); ++i) {
                if(i % 10 == 0) {
                    // Write (10% of operations)
                    spin_rw_mutex::scoped_lock write_lock(rw_mtx, true);
                    shared_counter++;
                    writes++;
                } else {
                    // Read (90% of operations)
                    spin_rw_mutex::scoped_lock read_lock(rw_mtx, false);
                    int val = shared_counter;  // Read
                    reads++;
                }
            }
        });
    
    auto elapsed = duration_cast<milliseconds>(
        high_resolution_clock::now() - start).count();
    
    cout << "Reads: " << reads << " (" << reads*100/N << "%)" << endl;
    cout << "Writes: " << writes << " (" << writes*100/N << "%)" << endl;
    cout << "Time: " << elapsed << " ms" << endl;
    cout << "RW lock is efficient for read-heavy workloads" << endl;
}

// ============= Example 4: Configuration Manager =============

class ConfigManager {
    spin_rw_mutex rw_mtx;
    map<string, string> config;
    
public:
    ConfigManager() {
        config["host"] = "localhost";
        config["port"] = "8080";
        config["timeout"] = "30";
    }
    
    string get(const string& key) {
        spin_rw_mutex::scoped_lock read_lock(rw_mtx, false);
        auto it = config.find(key);
        return (it != config.end()) ? it->second : "";
    }
    
    void set(const string& key, const string& value) {
        spin_rw_mutex::scoped_lock write_lock(rw_mtx, true);
        config[key] = value;
    }
    
    int size() {
        spin_rw_mutex::scoped_lock read_lock(rw_mtx, false);
        return config.size();
    }
};

void example4_config_manager() {
    cout << "\n=== Example 4: Configuration Manager ===" << endl;
    
    ConfigManager cfg;
    
    // Many threads reading config
    tbb::atomic<int> read_count;
    read_count = 0;
    
    thread readers([&]() {
        parallel_for(blocked_range<int>(0, 1000),
            [&](blocked_range<int> r) {
                for(int i = r.begin(); i != r.end(); ++i) {
                    string host = cfg.get("host");
                    string port = cfg.get("port");
                    read_count++;
                }
            });
    });
    
    // Occasional writes
    thread writer([&]() {
        for(int i = 0; i < 10; ++i) {
            this_thread::sleep_for(milliseconds(10));
            cfg.set("counter", to_string(i));
        }
    });
    
    readers.join();
    writer.join();
    
    cout << "Reads: " << read_count << endl;
    cout << "Config size: " << cfg.size() << endl;
    cout << "Readers don't block each other!" << endl;
}

// ============= Example 5: Performance Comparison =============

void example5_performance() {
    cout << "\n=== Example 5: RW Lock vs Regular Mutex ===" << endl;
    
    const int N = 10000;
    vector<int> data = {1, 2, 3, 4, 5};
    
    // With regular mutex
    tbb::mutex regular_mtx;
    auto start = high_resolution_clock::now();
    
    parallel_for(blocked_range<int>(0, N),
        [&](blocked_range<int> r) {
            for(int i = r.begin(); i != r.end(); ++i) {
                tbb::mutex::scoped_lock lock(regular_mtx);
                int sum = 0;
                for(int x : data) sum += x;
            }
        });
    
    auto mutex_time = duration_cast<milliseconds>(
        high_resolution_clock::now() - start).count();
    
    // With RW lock
    spin_rw_mutex rw_mtx;
    start = high_resolution_clock::now();
    
    parallel_for(blocked_range<int>(0, N),
        [&](blocked_range<int> r) {
            for(int i = r.begin(); i != r.end(); ++i) {
                spin_rw_mutex::scoped_lock read_lock(rw_mtx, false);
                int sum = 0;
                for(int x : data) sum += x;
            }
        });
    
    auto rw_time = duration_cast<milliseconds>(
        high_resolution_clock::now() - start).count();
    
    cout << "Regular mutex: " << mutex_time << " ms" << endl;
    cout << "RW lock:       " << rw_time << " ms" << endl;
    cout << "Speedup:       " << (double)mutex_time / rw_time << "x" << endl;
    cout << "RW lock allows concurrent reads!" << endl;
}

// ============= Example 6: Cache with Rare Updates =============

class ThreadSafeCache {
    queuing_rw_mutex rw_mtx;  // Fair RW lock
    map<int, string> cache;
    tbb::atomic<int> hits, misses, updates;
    
public:
    ThreadSafeCache() {
        hits = 0;
        misses = 0;
        updates = 0;
    }
    
    string get(int key) {
        queuing_rw_mutex::scoped_lock read_lock(rw_mtx, false);
        auto it = cache.find(key);
        if(it != cache.end()) {
            hits++;
            return it->second;
        }
        misses++;
        return "";
    }
    
    void put(int key, const string& value) {
        queuing_rw_mutex::scoped_lock write_lock(rw_mtx, true);
        cache[key] = value;
        updates++;
    }
    
    void stats() {
        cout << "  Cache size: " << cache.size() << endl;
        cout << "  Hits: " << hits << ", Misses: " << misses << endl;
        cout << "  Updates: " << updates << endl;
        cout << "  Hit rate: " << (100.0 * hits / (hits + misses + 1)) << "%" << endl;
    }
};

void example6_cache() {
    cout << "\n=== Example 6: Cache with Rare Updates ===" << endl;
    
    ThreadSafeCache cache;
    
    // Pre-populate
    for(int i = 0; i < 100; ++i) {
        cache.put(i, "Value_" + to_string(i));
    }
    
    // Many readers
    thread readers([&]() {
        parallel_for(blocked_range<int>(0, 10000),
            [&](blocked_range<int> r) {
                for(int i = r.begin(); i != r.end(); ++i) {
                    int key = rand() % 150;  // Some hits, some misses
                    string val = cache.get(key);
                }
            });
    });
    
    // Rare writer
    thread writer([&]() {
        for(int i = 100; i < 110; ++i) {
            this_thread::sleep_for(milliseconds(50));
            cache.put(i, "NewValue_" + to_string(i));
        }
    });
    
    readers.join();
    writer.join();
    
    cache.stats();
    cout << "RW lock perfect for read-heavy caches!" << endl;
}

// ============= Example 7: Upgrade Pattern =============

void example7_upgrade_pattern() {
    cout << "\n=== Example 7: Read-to-Write Upgrade Pattern ===" << endl;
    
    spin_rw_mutex rw_mtx;
    map<int, int> data;
    
    // Initialize
    for(int i = 0; i < 100; ++i) {
        data[i] = i;
    }
    
    cout << "Pattern: Read first, upgrade to write if needed" << endl;
    
    int key = 50;
    
    // Try read first
    {
        spin_rw_mutex::scoped_lock read_lock(rw_mtx, false);
        cout << "Checking if key " << key << " exists..." << endl;
        
        if(data.find(key) == data.end()) {
            // Need to insert - must upgrade to write
            read_lock.release();  // Release read lock
            
            spin_rw_mutex::scoped_lock write_lock(rw_mtx, true);
            // Check again (may have changed)
            if(data.find(key) == data.end()) {
                data[key] = key * 10;
                cout << "Inserted new value: " << data[key] << endl;
            }
        } else {
            cout << "Key exists: " << data[key] << endl;
        }
    }
    
    cout << "Note: Manual upgrade pattern required" << endl;
}

int main() {
    cout << "╔════════════════════════════════════════════════════════╗" << endl;
    cout << "║   TBB reader_writer_lock - Complete Tutorial          ║" << endl;
    cout << "╚════════════════════════════════════════════════════════╝" << endl;
    
    example1_basic();
    example2_concurrent_reads();
    example3_mixed();
    example4_config_manager();
    example5_performance();
    example6_cache();
    example7_upgrade_pattern();
    
    cout << "\n╔════════════════════════════════════════════════════════╗" << endl;
    cout << "║                    KEY TAKEAWAYS                       ║" << endl;
    cout << "╠════════════════════════════════════════════════════════╣" << endl;
    cout << "║                                                        ║" << endl;
    cout << "║  1. Multiple readers can hold lock concurrently       ║" << endl;
    cout << "║  2. Writers have exclusive access                     ║" << endl;
    cout << "║  3. scoped_lock(mtx, false) for read                  ║" << endl;
    cout << "║  4. scoped_lock(mtx, true) for write                  ║" << endl;
    cout << "║  5. Perfect for read-heavy workloads                  ║" << endl;
    cout << "║  6. spin_rw_mutex for short locks                     ║" << endl;
    cout << "║  7. queuing_rw_mutex for fairness                     ║" << endl;
    cout << "║                                                        ║" << endl;
    cout << "╠════════════════════════════════════════════════════════╣" << endl;
    cout << "║               COMMON PATTERNS                          ║" << endl;
    cout << "╠════════════════════════════════════════════════════════╣" << endl;
    cout << "║                                                        ║" << endl;
    cout << "║  Read:                                                 ║" << endl;
    cout << "║    spin_rw_mutex::scoped_lock lock(mtx, false);       ║" << endl;
    cout << "║    value = shared_data;  // Concurrent reads OK       ║" << endl;
    cout << "║                                                        ║" << endl;
    cout << "║  Write:                                                ║" << endl;
    cout << "║    spin_rw_mutex::scoped_lock lock(mtx, true);        ║" << endl;
    cout << "║    shared_data = new_value;  // Exclusive             ║" << endl;
    cout << "║                                                        ║" << endl;
    cout << "╠════════════════════════════════════════════════════════╣" << endl;
    cout << "║               WHEN TO USE                              ║" << endl;
    cout << "╠════════════════════════════════════════════════════════╣" << endl;
    cout << "║                                                        ║" << endl;
    cout << "║  ✓ Read-heavy workloads (>90% reads)                  ║" << endl;
    cout << "║  ✓ Shared configuration data                          ║" << endl;
    cout << "║  ✓ Lookup tables with rare updates                    ║" << endl;
    cout << "║  ✓ Caches with occasional invalidation                ║" << endl;
    cout << "║  ✗ Write-heavy workloads (use regular mutex)          ║" << endl;
    cout << "║  ✗ Equal read/write mix (use regular mutex)           ║" << endl;
    cout << "║                                                        ║" << endl;
    cout << "╚════════════════════════════════════════════════════════╝" << endl;
    
    return 0;
}
