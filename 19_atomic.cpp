/*
 * TBB ATOMIC - Lock-Free Atomic Operations
 * 
 * DEFINITION:
 * tbb::atomic provides lock-free atomic operations for basic types,
 * ensuring thread-safe access without explicit locking.
 * 
 * KEY FEATURES:
 * - Lock-free operations
 * - Compare-and-swap (CAS)
 * - Fetch-and-add, fetch-and-store
 * - Memory ordering control
 * - Works with basic types
 * 
 * WHEN TO USE:
 * - Counters and flags
 * - Lock-free algorithms
 * - Performance-critical sync
 * - Minimize contention
 * - Simple shared state
 * 
 * PERFORMANCE:
 * - Much faster than mutexes
 * - No context switching
 * - Cache-line aware
 * - Scales with cores
 */

#include <iostream>
#include <cmath>
#include <vector>
#include <thread>
#include <chrono>
#include <mutex>
#include <tbb/atomic.h>
#include <tbb/parallel_for.h>
#include <tbb/blocked_range.h>

using namespace std;
using namespace tbb;
using namespace chrono;

// ============= Example 1: Basic Atomic Counter =============

void example1_counter() {
    cout << "\n=== Example 1: Atomic Counter ===" << endl;
    
    tbb::atomic<int> counter;
    counter = 0;
    
    const int N = 10000;
    
    // Multiple threads incrementing
    parallel_for(blocked_range<int>(0, N),
        [&](blocked_range<int> r) {
            for(int i = r.begin(); i != r.end(); ++i) {
                counter++;  // Lock-free atomic increment
            }
        });
    
    cout << "Counter value: " << counter << endl;
    cout << "Expected: " << N << endl;
    cout << "Match: " << (counter == N ? "✓" : "✗") << endl;
}

// ============= Example 2: Performance vs Mutex =============

void example2_performance() {
    cout << "\n=== Example 2: Performance vs Mutex ===" << endl;
    
    const int N = 1000000;
    
    // With atomic
    tbb::atomic<int> atomic_counter;
    atomic_counter = 0;
    
    auto start = high_resolution_clock::now();
    parallel_for(blocked_range<int>(0, N),
        [&](blocked_range<int> r) {
            for(int i = r.begin(); i != r.end(); ++i)
                atomic_counter++;
        });
    auto atomic_time = duration_cast<milliseconds>(
        high_resolution_clock::now() - start).count();
    
    // With mutex (for comparison)
    mutex mtx;
    int mutex_counter = 0;
    
    start = high_resolution_clock::now();
    parallel_for(blocked_range<int>(0, N),
        [&](blocked_range<int> r) {
            for(int i = r.begin(); i != r.end(); ++i) {
                lock_guard<mutex> lock(mtx);
                mutex_counter++;
            }
        });
    auto mutex_time = duration_cast<milliseconds>(
        high_resolution_clock::now() - start).count();
    
    cout << "Atomic: " << atomic_time << " ms" << endl;
    cout << "Mutex:  " << mutex_time << " ms" << endl;
    cout << "Speedup: " << (double)mutex_time / atomic_time << "x" << endl;
}

// ============= Example 3: Compare and Swap (CAS) =============

void example3_compare_and_swap() {
    cout << "\n=== Example 3: Compare and Swap ===" << endl;
    
    tbb::atomic<int> value;
    value = 100;
    
    cout << "Initial value: " << value << endl;
    
    // CAS: if value == 100, set to 200
    int old_val = 100;
    int new_val = 200;
    
    int result = value.compare_and_swap(new_val, old_val);
    
    if(result == old_val) {
        cout << "CAS succeeded: value is now " << value << endl;
    } else {
        cout << "CAS failed: value was " << result << endl;
    }
    
    // Try again with wrong old value
    old_val = 100;  // Wrong!
    new_val = 300;
    result = value.compare_and_swap(new_val, old_val);
    
    if(result == old_val) {
        cout << "Second CAS succeeded" << endl;
    } else {
        cout << "Second CAS failed: expected " << old_val 
             << " but was " << result << endl;
    }
}

// ============= Example 4: Fetch and Add =============

void example4_fetch_and_add() {
    cout << "\n=== Example 4: Fetch and Add ===" << endl;
    
    tbb::atomic<int> counter;
    counter = 0;
    
    // fetch_and_add returns OLD value
    int old1 = counter.fetch_and_add(5);
    cout << "fetch_and_add(5) returned: " << old1 << endl;
    cout << "Counter is now: " << counter << endl;
    
    int old2 = counter.fetch_and_add(10);
    cout << "fetch_and_add(10) returned: " << old2 << endl;
    cout << "Counter is now: " << counter << endl;
    
    // fetch_and_increment
    int old3 = counter.fetch_and_increment();
    cout << "fetch_and_increment() returned: " << old3 << endl;
    cout << "Counter is now: " << counter << endl;
}

// ============= Example 5: Atomic Flags =============

void example5_flags() {
    cout << "\n=== Example 5: Atomic Flags ===" << endl;
    
    tbb::atomic<bool> ready;
    tbb::atomic<bool> done;
    ready = false;
    done = false;
    
    tbb::atomic<int> result;
    result = 0;
    
    // Worker thread
    thread worker([&]() {
        // Wait for ready signal
        while(!ready) {
            this_thread::yield();
        }
        
        cout << "Worker: Processing..." << endl;
        
        // Do work
        for(int i = 0; i < 1000; ++i) {
            result += i;
        }
        
        done = true;
    });
    
    // Main thread
    this_thread::sleep_for(milliseconds(10));
    cout << "Main: Signaling worker to start" << endl;
    ready = true;
    
    // Wait for completion
    while(!done) {
        this_thread::yield();
    }
    
    cout << "Result: " << result << endl;
    worker.join();
}

// ============= Example 6: Lock-Free Stack (Simplified) =============

template<typename T>
class LockFreeStack {
    struct Node {
        T value;
        Node* next;
        Node(T v) : value(v), next(nullptr) {}
    };
    
    tbb::atomic<Node*> head;
    
public:
    LockFreeStack() {
        head = nullptr;
    }
    
    void push(T value) {
        Node* new_node = new Node(value);
        Node* old_head;
        
        do {
            old_head = head;
            new_node->next = old_head;
        } while(head.compare_and_swap(new_node, old_head) != old_head);
    }
    
    bool try_pop(T& value) {
        Node* old_head;
        Node* new_head;
        
        do {
            old_head = head;
            if(old_head == nullptr)
                return false;
            new_head = old_head->next;
        } while(head.compare_and_swap(new_head, old_head) != old_head);
        
        value = old_head->value;
        delete old_head;
        return true;
    }
};

void example6_lock_free_stack() {
    cout << "\n=== Example 6: Lock-Free Stack ===" << endl;
    
    LockFreeStack<int> stack;
    
    // Push items
    for(int i = 0; i < 10; ++i) {
        stack.push(i);
    }
    
    cout << "Pushed 10 items" << endl;
    
    // Pop items
    cout << "Popping: ";
    int value;
    while(stack.try_pop(value)) {
        cout << value << " ";
    }
    cout << endl;
    cout << "Lock-free data structure using atomic CAS!" << endl;
}

// ============= Example 7: Parallel Accumulation =============

void example7_accumulation() {
    cout << "\n=== Example 7: Parallel Accumulation ===" << endl;
    
    const int N = 1000000;
    vector<int> data(N);
    for(int i = 0; i < N; ++i) data[i] = i % 100;
    
    tbb::atomic<long long> sum;
    sum = 0;
    
    auto start = high_resolution_clock::now();
    
    parallel_for(blocked_range<int>(0, N),
        [&](blocked_range<int> r) {
            long long local_sum = 0;
            for(int i = r.begin(); i != r.end(); ++i) {
                local_sum += data[i];
            }
            // Atomic add of local sum
            sum.fetch_and_add(local_sum);
        });
    
    auto elapsed = duration_cast<microseconds>(
        high_resolution_clock::now() - start).count();
    
    cout << "Sum: " << sum << endl;
    cout << "Time: " << elapsed << " μs" << endl;
    cout << "Using atomic for final accumulation" << endl;
}

int main() {
    cout << "╔════════════════════════════════════════════════════════╗" << endl;
    cout << "║      TBB atomic - Complete Tutorial                   ║" << endl;
    cout << "╚════════════════════════════════════════════════════════╝" << endl;
    
    example1_counter();
    example2_performance();
    example3_compare_and_swap();
    example4_fetch_and_add();
    example5_flags();
    example6_lock_free_stack();
    example7_accumulation();
    
    cout << "\n╔════════════════════════════════════════════════════════╗" << endl;
    cout << "║                    KEY TAKEAWAYS                       ║" << endl;
    cout << "╠════════════════════════════════════════════════════════╣" << endl;
    cout << "║                                                        ║" << endl;
    cout << "║  1. Lock-free atomic operations                       ║" << endl;
    cout << "║  2. Much faster than mutexes for simple operations    ║" << endl;
    cout << "║  3. compare_and_swap for lock-free algorithms         ║" << endl;
    cout << "║  4. fetch_and_add returns OLD value                   ║" << endl;
    cout << "║  5. Perfect for counters and flags                    ║" << endl;
    cout << "║  6. Enables lock-free data structures                 ║" << endl;
    cout << "║  7. No context switching overhead                     ║" << endl;
    cout << "║                                                        ║" << endl;
    cout << "╠════════════════════════════════════════════════════════╣" << endl;
    cout << "║               COMMON PATTERNS                          ║" << endl;
    cout << "╠════════════════════════════════════════════════════════╣" << endl;
    cout << "║                                                        ║" << endl;
    cout << "║  Counter:                                              ║" << endl;
    cout << "║    tbb::atomic<int> counter;                          ║" << endl;
    cout << "║    counter++;  // Lock-free                           ║" << endl;
    cout << "║                                                        ║" << endl;
    cout << "║  Flag:                                                 ║" << endl;
    cout << "║    tbb::atomic<bool> ready;                           ║" << endl;
    cout << "║    ready = true;                                      ║" << endl;
    cout << "║                                                        ║" << endl;
    cout << "║  Compare-and-Swap:                                     ║" << endl;
    cout << "║    tbb::atomic<T> value;                              ║" << endl;
    cout << "║    T old = expected;                                  ║" << endl;
    cout << "║    if(value.compare_and_swap(new_val, old) == old)   ║" << endl;
    cout << "║        // Success                                      ║" << endl;
    cout << "║                                                        ║" << endl;
    cout << "╠════════════════════════════════════════════════════════╣" << endl;
    cout << "║               WHEN TO USE                              ║" << endl;
    cout << "╠════════════════════════════════════════════════════════╣" << endl;
    cout << "║                                                        ║" << endl;
    cout << "║  ✓ Simple shared counters                             ║" << endl;
    cout << "║  ✓ Flags and boolean states                           ║" << endl;
    cout << "║  ✓ Lock-free algorithms (stacks, queues)              ║" << endl;
    cout << "║  ✓ Performance-critical synchronization               ║" << endl;
    cout << "║  ✓ Minimize contention                                ║" << endl;
    cout << "║  ✗ Complex data structures (use mutex)                ║" << endl;
    cout << "║  ✗ Need to protect multiple operations (use mutex)    ║" << endl;
    cout << "║                                                        ║" << endl;
    cout << "╚════════════════════════════════════════════════════════╝" << endl;
    
    return 0;
}
