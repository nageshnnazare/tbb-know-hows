/*
 * TBB MUTEX - Mutual Exclusion Locks
 * 
 * DEFINITION:
 * TBB provides several mutex types for protecting shared data from
 * concurrent access. Each has different characteristics for different use cases.
 * 
 * MUTEX TYPES:
 * - mutex: Fair, scalable, default choice
 * - spin_mutex: Busy-wait, for very short critical sections
 * - queuing_mutex: FIFO ordering, guaranteed fairness
 * - spin_rw_mutex: Reader-writer lock with spinning
 * - queuing_rw_mutex: Reader-writer lock with queuing
 * 
 * WHEN TO USE:
 * - Protect shared mutable state
 * - Critical sections
 * - Coordinate threads
 * - Serialize access to resources
 * 
 * SCOPED_LOCK PATTERN:
 * - RAII-based lock acquisition
 * - Automatic unlock on scope exit
 * - Exception-safe
 */

#include <iostream>
#include <cmath>
#include <vector>
#include <thread>
#include <chrono>
#include <tbb/mutex.h>
#include <tbb/spin_mutex.h>
#include <tbb/queuing_mutex.h>
#include <tbb/parallel_for.h>
#include <tbb/blocked_range.h>

using namespace std;
using namespace tbb;
using namespace chrono;

// ============= Example 1: Basic tbb::mutex =============

void example1_basic_mutex() {
    cout << "\n=== Example 1: Basic tbb::mutex ===" << endl;
    
    tbb::mutex mtx;
    int shared_counter = 0;
    vector<int> shared_log;
    
    const int N = 1000;
    
    auto start = high_resolution_clock::now();
    
    parallel_for(blocked_range<int>(0, N),
        [&](blocked_range<int> r) {
            for(int i = r.begin(); i != r.end(); ++i) {
                tbb::mutex::scoped_lock lock(mtx);
                shared_counter++;
                if(shared_counter % 100 == 0) {
                    shared_log.push_back(shared_counter);
                }
            }
        });
    
    auto elapsed = duration_cast<milliseconds>(
        high_resolution_clock::now() - start).count();
    
    cout << "Counter: " << shared_counter << endl;
    cout << "Time: " << elapsed << " ms" << endl;
    cout << "Log entries: " << shared_log.size() << endl;
}

// ============= Example 2: Scoped Lock Pattern =============

void example2_scoped_lock() {
    cout << "\n=== Example 2: Scoped Lock Pattern ===" << endl;
    
    tbb::mutex mtx;
    int balance = 1000;
    
    auto deposit = [&](int amount) {
        tbb::mutex::scoped_lock lock(mtx);
        balance += amount;
        cout << "Deposited " << amount << ", balance: " << balance << endl;
    }; // Lock automatically released
    
    auto withdraw = [&](int amount) {
        tbb::mutex::scoped_lock lock(mtx);
        if(balance >= amount) {
            balance -= amount;
            cout << "Withdrew " << amount << ", balance: " << balance << endl;
        } else {
            cout << "Insufficient funds for " << amount << endl;
        }
    };
    
    vector<thread> threads;
    threads.emplace_back([&]() { deposit(500); });
    threads.emplace_back([&]() { withdraw(300); });
    threads.emplace_back([&]() { deposit(200); });
    threads.emplace_back([&]() { withdraw(1000); });
    
    for(auto& t : threads) t.join();
    
    cout << "Final balance: " << balance << endl;
}

// ============= Example 3: Multiple Mutexes =============

void example3_multiple_mutexes() {
    cout << "\n=== Example 3: Multiple Mutexes ===" << endl;
    
    tbb::mutex mtx1, mtx2;
    int counter1 = 0, counter2 = 0;
    
    parallel_for(blocked_range<int>(0, 1000),
        [&](blocked_range<int> r) {
            for(int i = r.begin(); i != r.end(); ++i) {
                if(i % 2 == 0) {
                    tbb::mutex::scoped_lock lock(mtx1);
                    counter1++;
                } else {
                    tbb::mutex::scoped_lock lock(mtx2);
                    counter2++;
                }
            }
        });
    
    cout << "Counter 1 (even): " << counter1 << endl;
    cout << "Counter 2 (odd):  " << counter2 << endl;
    cout << "Using separate mutexes reduces contention" << endl;
}

// ============= Example 4: Try-Lock Pattern =============

void example4_try_lock() {
    cout << "\n=== Example 4: Try-Lock Pattern ===" << endl;
    
    tbb::mutex mtx;
    int resource = 0;
    tbb::atomic<int> failed_attempts;
    failed_attempts = 0;
    
    parallel_for(blocked_range<int>(0, 100),
        [&](blocked_range<int> r) {
            for(int i = r.begin(); i != r.end(); ++i) {
                tbb::mutex::scoped_lock lock;
                if(lock.try_acquire(mtx)) {
                    // Got the lock
                    resource++;
                    this_thread::sleep_for(microseconds(100));
                    lock.release();
                } else {
                    // Couldn't get lock
                    failed_attempts++;
                }
            }
        });
    
    cout << "Resource value: " << resource << endl;
    cout << "Failed attempts: " << failed_attempts << endl;
    cout << "try_acquire() returns false if lock unavailable" << endl;
}

// ============= Example 5: Comparison of Mutex Types =============

void example5_mutex_comparison() {
    cout << "\n=== Example 5: Mutex Type Comparison ===" << endl;
    
    const int N = 100000;
    
    // tbb::mutex (fair, scalable)
    {
        tbb::mutex mtx;
        int counter = 0;
        
        auto start = high_resolution_clock::now();
        parallel_for(blocked_range<int>(0, N),
            [&](blocked_range<int> r) {
                for(int i = r.begin(); i != r.end(); ++i) {
                    tbb::mutex::scoped_lock lock(mtx);
                    counter++;
                }
            });
        auto time = duration_cast<milliseconds>(
            high_resolution_clock::now() - start).count();
        
        cout << "tbb::mutex:      " << time << " ms" << endl;
    }
    
    // spin_mutex (busy-wait)
    {
        spin_mutex mtx;
        int counter = 0;
        
        auto start = high_resolution_clock::now();
        parallel_for(blocked_range<int>(0, N),
            [&](blocked_range<int> r) {
                for(int i = r.begin(); i != r.end(); ++i) {
                    spin_mutex::scoped_lock lock(mtx);
                    counter++;
                }
            });
        auto time = duration_cast<milliseconds>(
            high_resolution_clock::now() - start).count();
        
        cout << "spin_mutex:      " << time << " ms" << endl;
    }
    
    // queuing_mutex (fair FIFO)
    {
        queuing_mutex mtx;
        int counter = 0;
        
        auto start = high_resolution_clock::now();
        parallel_for(blocked_range<int>(0, N),
            [&](blocked_range<int> r) {
                for(int i = r.begin(); i != r.end(); ++i) {
                    queuing_mutex::scoped_lock lock(mtx);
                    counter++;
                }
            });
        auto time = duration_cast<milliseconds>(
            high_resolution_clock::now() - start).count();
        
        cout << "queuing_mutex:   " << time << " ms" << endl;
    }
    
    cout << "\nChoose based on use case:" << endl;
    cout << "  mutex: Default choice, good balance" << endl;
    cout << "  spin_mutex: Very short critical sections" << endl;
    cout << "  queuing_mutex: When fairness matters" << endl;
}

// ============= Example 6: Protecting Complex Operations =============

struct BankAccount {
    tbb::mutex mtx;
    int balance;
    vector<string> transaction_log;
    
    BankAccount() : balance(1000) {}
    
    bool transfer(int amount, const string& description) {
        tbb::mutex::scoped_lock lock(mtx);
        
        if(balance >= amount) {
            balance -= amount;
            transaction_log.push_back(description);
            return true;
        }
        return false;
    }
    
    int get_balance() {
        tbb::mutex::scoped_lock lock(mtx);
        return balance;
    }
    
    int get_transaction_count() {
        tbb::mutex::scoped_lock lock(mtx);
        return transaction_log.size();
    }
};

void example6_complex_protection() {
    cout << "\n=== Example 6: Protecting Complex Operations ===" << endl;
    
    BankAccount account;
    
    // Concurrent transactions
    parallel_for(blocked_range<int>(0, 100),
        [&](blocked_range<int> r) {
            for(int i = r.begin(); i != r.end(); ++i) {
                account.transfer(10, "Transaction_" + to_string(i));
            }
        });
    
    cout << "Final balance: " << account.get_balance() << endl;
    cout << "Transactions: " << account.get_transaction_count() << endl;
    cout << "Mutex protects both balance and log consistently" << endl;
}

// ============= Example 7: Deadlock Avoidance =============

void example7_deadlock_avoidance() {
    cout << "\n=== Example 7: Deadlock Avoidance ===" << endl;
    
    tbb::mutex mtx1, mtx2;
    int resource1 = 0, resource2 = 0;
    
    cout << "Good practice: Always lock in same order" << endl;
    
    // Thread 1: locks mtx1 then mtx2
    thread t1([&]() {
        for(int i = 0; i < 100; ++i) {
            tbb::mutex::scoped_lock lock1(mtx1);  // Always first
            tbb::mutex::scoped_lock lock2(mtx2);  // Always second
            resource1++;
            resource2++;
        }
    });
    
    // Thread 2: also locks mtx1 then mtx2 (same order)
    thread t2([&]() {
        for(int i = 0; i < 100; ++i) {
            tbb::mutex::scoped_lock lock1(mtx1);  // Same order
            tbb::mutex::scoped_lock lock2(mtx2);
            resource1++;
            resource2++;
        }
    });
    
    t1.join();
    t2.join();
    
    cout << "Resource 1: " << resource1 << endl;
    cout << "Resource 2: " << resource2 << endl;
    cout << "No deadlock - consistent lock ordering!" << endl;
}

int main() {
    cout << "╔════════════════════════════════════════════════════════╗" << endl;
    cout << "║      TBB mutex - Complete Tutorial                    ║" << endl;
    cout << "╚════════════════════════════════════════════════════════╝" << endl;
    
    example1_basic_mutex();
    example2_scoped_lock();
    example3_multiple_mutexes();
    example4_try_lock();
    example5_mutex_comparison();
    example6_complex_protection();
    example7_deadlock_avoidance();
    
    cout << "\n╔════════════════════════════════════════════════════════╗" << endl;
    cout << "║                    KEY TAKEAWAYS                       ║" << endl;
    cout << "╠════════════════════════════════════════════════════════╣" << endl;
    cout << "║                                                        ║" << endl;
    cout << "║  1. Use scoped_lock for RAII-based locking            ║" << endl;
    cout << "║  2. mutex: Default choice, fair and scalable          ║" << endl;
    cout << "║  3. spin_mutex: For very short critical sections      ║" << endl;
    cout << "║  4. queuing_mutex: When fairness required             ║" << endl;
    cout << "║  5. Lock is released automatically on scope exit      ║" << endl;
    cout << "║  6. try_acquire() for non-blocking lock attempts      ║" << endl;
    cout << "║  7. Always lock multiple mutexes in same order        ║" << endl;
    cout << "║                                                        ║" << endl;
    cout << "╠════════════════════════════════════════════════════════╣" << endl;
    cout << "║               COMMON PATTERNS                          ║" << endl;
    cout << "╠════════════════════════════════════════════════════════╣" << endl;
    cout << "║                                                        ║" << endl;
    cout << "║  Basic Protection:                                     ║" << endl;
    cout << "║    tbb::mutex mtx;                                    ║" << endl;
    cout << "║    {                                                   ║" << endl;
    cout << "║        tbb::mutex::scoped_lock lock(mtx);             ║" << endl;
    cout << "║        shared_data++;  // Protected                   ║" << endl;
    cout << "║    }  // Auto-unlock                                   ║" << endl;
    cout << "║                                                        ║" << endl;
    cout << "║  Try-Lock:                                             ║" << endl;
    cout << "║    tbb::mutex::scoped_lock lock;                      ║" << endl;
    cout << "║    if(lock.try_acquire(mtx)) {                        ║" << endl;
    cout << "║        // Got lock                                     ║" << endl;
    cout << "║    }                                                   ║" << endl;
    cout << "║                                                        ║" << endl;
    cout << "╠════════════════════════════════════════════════════════╣" << endl;
    cout << "║               WHEN TO USE                              ║" << endl;
    cout << "╠════════════════════════════════════════════════════════╣" << endl;
    cout << "║                                                        ║" << endl;
    cout << "║  ✓ Protect shared mutable state                       ║" << endl;
    cout << "║  ✓ Serialize access to resources                      ║" << endl;
    cout << "║  ✓ Coordinate multiple threads                        ║" << endl;
    cout << "║  ✓ Exception-safe synchronization                     ║" << endl;
    cout << "║  ✗ Simple counters (use atomic instead)               ║" << endl;
    cout << "║  ✗ Reader-heavy workloads (use reader-writer lock)    ║" << endl;
    cout << "║  ✗ Can avoid with concurrent containers               ║" << endl;
    cout << "║                                                        ║" << endl;
    cout << "╚════════════════════════════════════════════════════════╝" << endl;
    
    return 0;
}
