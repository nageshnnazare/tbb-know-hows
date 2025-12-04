/*
 * TBB QUEUING_MUTEX - Fair FIFO Mutex
 * 
 * DEFINITION:
 * queuing_mutex is a mutex that grants lock requests in FIFO order,
 * providing guaranteed fairness at the cost of slightly higher overhead.
 * 
 * KEY FEATURES:
 * - FIFO ordering (first to request, first to acquire)
 * - No thread starvation
 * - Predictable latency
 * - Fair scheduling
 * 
 * COMPARISON:
 * - mutex: Fair but not strictly FIFO
 * - spin_mutex: Not fair, can cause starvation
 * - queuing_mutex: Strictly FIFO, guaranteed fairness
 * 
 * WHEN TO USE:
 * - Fairness is critical
 * - Prevent thread starvation
 * - Predictable response times needed
 * - Load balancing across threads
 * 
 * TRADE-OFFS:
 * + Guaranteed fairness
 * + No starvation
 * - Slightly higher overhead
 * - More memory usage
 */

#include <iostream>
#include <cmath>
#include <vector>
#include <thread>
#include <chrono>
#include <tbb/queuing_mutex.h>
#include <tbb/mutex.h>
#include <tbb/spin_mutex.h>
#include <tbb/parallel_for.h>
#include <tbb/blocked_range.h>

using namespace std;
using namespace tbb;
using namespace chrono;

// ============= Example 1: Basic queuing_mutex =============

void example1_basic() {
    cout << "\n=== Example 1: Basic queuing_mutex ===" << endl;
    
    queuing_mutex mtx;
    int shared_counter = 0;
    
    parallel_for(blocked_range<int>(0, 1000),
        [&](blocked_range<int> r) {
            for(int i = r.begin(); i != r.end(); ++i) {
                queuing_mutex::scoped_lock lock(mtx);
                shared_counter++;
            }
        });
    
    cout << "Counter: " << shared_counter << endl;
    cout << "Locks acquired in FIFO order" << endl;
}

// ============= Example 2: Fairness Demonstration =============

void example2_fairness() {
    cout << "\n=== Example 2: Fairness Guarantee ===" << endl;
    
    queuing_mutex mtx;
    vector<tbb::atomic<int>> thread_access_count(8);
    for(auto& counter : thread_access_count) {
        counter = 0;
    }
    
    const int ITERATIONS = 100;
    
    // Each thread tries to access the critical section many times
    parallel_for(blocked_range<int>(0, 8),
        [&](blocked_range<int> r) {
            int thread_id = r.begin();
            for(int i = 0; i < ITERATIONS; ++i) {
                queuing_mutex::scoped_lock lock(mtx);
                thread_access_count[thread_id]++;
                // Simulate some work
                this_thread::sleep_for(microseconds(10));
            }
        }, simple_partitioner());
    
    cout << "Access counts (should be fairly distributed):" << endl;
    for(size_t i = 0; i < thread_access_count.size(); ++i) {
        cout << "  Thread " << i << ": " << thread_access_count[i] << " times" << endl;
    }
    cout << "queuing_mutex ensures no thread is starved" << endl;
}

// ============= Example 3: Comparing Fairness =============

template<typename MutexType>
void test_mutex_fairness(const string& name, MutexType& mtx, vector<tbb::atomic<int>>& counters) {
    parallel_for(blocked_range<int>(0, 4),
        [&](blocked_range<int> r) {
            int thread_id = r.begin();
            for(int i = 0; i < 100; ++i) {
                typename MutexType::scoped_lock lock(mtx);
                counters[thread_id]++;
                this_thread::sleep_for(microseconds(5));
            }
        }, simple_partitioner());
}

void example3_fairness_comparison() {
    cout << "\n=== Example 3: Fairness Comparison ===" << endl;
    
    // Test with regular mutex
    {
        tbb::mutex mtx;
        vector<tbb::atomic<int>> counters(4);
        for(auto& c : counters) c = 0;
        
        test_mutex_fairness("tbb::mutex", mtx, counters);
        
        cout << "tbb::mutex distribution:" << endl;
        for(size_t i = 0; i < counters.size(); ++i) {
            cout << "  Thread " << i << ": " << counters[i] << endl;
        }
    }
    
    // Test with queuing_mutex
    {
        queuing_mutex mtx;
        vector<tbb::atomic<int>> counters(4);
        for(auto& c : counters) c = 0;
        
        test_mutex_fairness("queuing_mutex", mtx, counters);
        
        cout << "queuing_mutex distribution:" << endl;
        for(size_t i = 0; i < counters.size(); ++i) {
            cout << "  Thread " << i << ": " << counters[i] << endl;
        }
    }
    
    cout << "queuing_mutex provides more even distribution" << endl;
}

// ============= Example 4: Response Time Predictability =============

void example4_predictability() {
    cout << "\n=== Example 4: Response Time Predictability ===" << endl;
    
    queuing_mutex mtx;
    vector<long long> latencies;
    tbb::spin_mutex latencies_mtx;
    
    const int N = 100;
    
    parallel_for(blocked_range<int>(0, N),
        [&](blocked_range<int> r) {
            for(int i = r.begin(); i != r.end(); ++i) {
                auto request_time = high_resolution_clock::now();
                
                queuing_mutex::scoped_lock lock(mtx);
                
                auto acquired_time = high_resolution_clock::now();
                auto latency = duration_cast<microseconds>(
                    acquired_time - request_time).count();
                
                // Record latency
                {
                    tbb::spin_mutex::scoped_lock l(latencies_mtx);
                    latencies.push_back(latency);
                }
                
                // Simulate work
                this_thread::sleep_for(microseconds(100));
            }
        });
    
    // Calculate statistics
    long long sum = 0, min_lat = LLONG_MAX, max_lat = 0;
    for(auto lat : latencies) {
        sum += lat;
        min_lat = min(min_lat, lat);
        max_lat = max(max_lat, lat);
    }
    
    cout << "Latencies (time to acquire lock):" << endl;
    cout << "  Average: " << (sum / latencies.size()) << " μs" << endl;
    cout << "  Min: " << min_lat << " μs" << endl;
    cout << "  Max: " << max_lat << " μs" << endl;
    cout << "FIFO ensures predictable wait times" << endl;
}

// ============= Example 5: Ticket Booking System =============

class TicketSystem {
    queuing_mutex mtx;  // Fair ordering
    int available_tickets;
    vector<string> bookings;
    
public:
    TicketSystem(int tickets) : available_tickets(tickets) {}
    
    bool book_ticket(const string& customer) {
        queuing_mutex::scoped_lock lock(mtx);
        
        if(available_tickets > 0) {
            available_tickets--;
            bookings.push_back(customer);
            return true;
        }
        return false;
    }
    
    void report() {
        queuing_mutex::scoped_lock lock(mtx);
        cout << "Total bookings: " << bookings.size() << endl;
        cout << "Remaining tickets: " << available_tickets << endl;
    }
};

void example5_ticket_booking() {
    cout << "\n=== Example 5: Fair Ticket Booking ===" << endl;
    
    TicketSystem tickets(100);
    
    tbb::atomic<int> successful, failed;
    successful = 0;
    failed = 0;
    
    // Many customers trying to book concurrently
    parallel_for(blocked_range<int>(0, 150),
        [&](blocked_range<int> r) {
            for(int i = r.begin(); i != r.end(); ++i) {
                string customer = "Customer_" + to_string(i);
                if(tickets.book_ticket(customer)) {
                    successful++;
                } else {
                    failed++;
                }
            }
        });
    
    cout << "Successful bookings: " << successful << endl;
    cout << "Failed bookings: " << failed << endl;
    tickets.report();
    cout << "FIFO ensures fair first-come-first-served" << endl;
}

// ============= Example 6: Performance Overhead =============

void example6_performance() {
    cout << "\n=== Example 6: Performance Overhead ===" << endl;
    
    const int N = 10000;
    int counter = 0;
    
    // Test queuing_mutex
    {
        queuing_mutex mtx;
        counter = 0;
        
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
        
        cout << "queuing_mutex: " << time << " ms" << endl;
    }
    
    // Test regular mutex
    {
        tbb::mutex mtx;
        counter = 0;
        
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
        
        cout << "tbb::mutex:    " << time << " ms" << endl;
    }
    
    // Test spin_mutex
    {
        spin_mutex mtx;
        counter = 0;
        
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
        
        cout << "spin_mutex:    " << time << " ms" << endl;
    }
    
    cout << "\nqueuing_mutex has slight overhead for fairness guarantee" << endl;
}

// ============= Example 7: Producer-Consumer with Fair Access =============

void example7_producer_consumer() {
    cout << "\n=== Example 7: Fair Producer-Consumer ===" << endl;
    
    queuing_mutex mtx;
    vector<int> buffer;
    const int MAX_BUFFER = 10;
    bool done = false;
    
    // Multiple producers
    vector<thread> producers;
    for(int p = 0; p < 3; ++p) {
        producers.emplace_back([&, p]() {
            for(int i = 0; i < 10; ++i) {
                queuing_mutex::scoped_lock lock(mtx);
                if(buffer.size() < MAX_BUFFER) {
                    buffer.push_back(p * 100 + i);
                    cout << "Producer " << p << " added: " << (p * 100 + i) << endl;
                }
            }
        });
    }
    
    // Wait for producers
    for(auto& t : producers) t.join();
    
    {
        queuing_mutex::scoped_lock lock(mtx);
        done = true;
        cout << "Final buffer size: " << buffer.size() << endl;
    }
    
    cout << "Fair access ensured across all producers" << endl;
}

int main() {
    cout << "╔════════════════════════════════════════════════════════╗" << endl;
    cout << "║     TBB queuing_mutex - Complete Tutorial             ║" << endl;
    cout << "╚════════════════════════════════════════════════════════╝" << endl;
    
    example1_basic();
    example2_fairness();
    example3_fairness_comparison();
    example4_predictability();
    example5_ticket_booking();
    example6_performance();
    example7_producer_consumer();
    
    cout << "\n╔════════════════════════════════════════════════════════╗" << endl;
    cout << "║                    KEY TAKEAWAYS                       ║" << endl;
    cout << "╠════════════════════════════════════════════════════════╣" << endl;
    cout << "║                                                        ║" << endl;
    cout << "║  1. Strictly FIFO lock acquisition order              ║" << endl;
    cout << "║  2. Guaranteed fairness - no starvation               ║" << endl;
    cout << "║  3. Predictable response times                        ║" << endl;
    cout << "║  4. Higher overhead than regular mutex                ║" << endl;
    cout << "║  5. Perfect for first-come-first-served systems       ║" << endl;
    cout << "║  6. Use when fairness is critical                     ║" << endl;
    cout << "║  7. More memory than other mutex types                ║" << endl;
    cout << "║                                                        ║" << endl;
    cout << "╠════════════════════════════════════════════════════════╣" << endl;
    cout << "║               COMMON PATTERNS                          ║" << endl;
    cout << "╠════════════════════════════════════════════════════════╣" << endl;
    cout << "║                                                        ║" << endl;
    cout << "║  Basic Usage:                                          ║" << endl;
    cout << "║    queuing_mutex mtx;                                 ║" << endl;
    cout << "║    {                                                   ║" << endl;
    cout << "║        queuing_mutex::scoped_lock lock(mtx);          ║" << endl;
    cout << "║        // Fair FIFO access                             ║" << endl;
    cout << "║    }                                                   ║" << endl;
    cout << "║                                                        ║" << endl;
    cout << "╠════════════════════════════════════════════════════════╣" << endl;
    cout << "║               WHEN TO USE                              ║" << endl;
    cout << "╠════════════════════════════════════════════════════════╣" << endl;
    cout << "║                                                        ║" << endl;
    cout << "║  ✓ Fairness is required                               ║" << endl;
    cout << "║  ✓ Prevent thread starvation                          ║" << endl;
    cout << "║  ✓ First-come-first-served systems                    ║" << endl;
    cout << "║  ✓ Predictable latency needed                         ║" << endl;
    cout << "║  ✓ QoS guarantees                                     ║" << endl;
    cout << "║  ✗ Performance is critical (use mutex)                ║" << endl;
    cout << "║  ✗ Fairness not important (use spin_mutex)            ║" << endl;
    cout << "║                                                        ║" << endl;
    cout << "╚════════════════════════════════════════════════════════╝" << endl;
    
    return 0;
}
