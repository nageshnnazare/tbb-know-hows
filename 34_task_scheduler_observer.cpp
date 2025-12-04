/*
 * TBB TASK_SCHEDULER_OBSERVER - Monitor Thread Lifecycle
 * 
 * DEFINITION:
 * task_scheduler_observer allows you to observe and respond to
 * thread entry/exit events in the TBB task scheduler.
 * 
 * KEY METHODS:
 * - on_scheduler_entry(bool): Called when thread joins scheduler
 * - on_scheduler_exit(bool): Called when thread leaves scheduler
 * - observe(bool): Enable/disable observation
 * 
 * COMMON USE CASES:
 * - Thread-local initialization (pinning, affinity)
 * - Performance monitoring and profiling
 * - Resource management per thread
 * - Custom thread naming
 * - Integration with other threading systems
 * - Debug tracing
 * 
 * PARAMETERS:
 * - is_worker: true for TBB worker threads, false for master thread
 * 
 * WHEN TO USE:
 * - Need thread-specific setup/teardown
 * - Performance profiling
 * - CPU affinity/pinning
 * - Thread-local resource management
 * - Debugging thread behavior
 */

#include <iostream>
#include <cmath>
#include <thread>
#include <atomic>
#include <vector>
#include <chrono>
#include <tbb/task_scheduler_observer.h>
#include <tbb/enumerable_thread_specific.h>
#include <tbb/spin_mutex.h>
#include <tbb/task_arena.h>
#include <tbb/parallel_for.h>
#include <tbb/blocked_range.h>

using namespace std;
using std::atomic;
using namespace tbb;
using namespace chrono;

// ============= Example 1: Basic Observer =============

class SimpleObserver : public task_scheduler_observer {
    std::atomic<int> entry_count;
    std::atomic<int> exit_count;
    
public:
    SimpleObserver() : entry_count(0), exit_count(0) {
        observe(true);  // Start observing
    }
    
    void on_scheduler_entry(bool is_worker) override {
        entry_count++;
        cout << "  Thread " << this_thread::get_id() << " entered "
             << (is_worker ? "(worker)" : "(master)") << endl;
    }
    
    void on_scheduler_exit(bool is_worker) override {
        exit_count++;
        cout << "  Thread " << this_thread::get_id() << " exited "
             << (is_worker ? "(worker)" : "(master)") << endl;
    }
    
    void print_stats() {
        cout << "Total entries: " << entry_count << endl;
        cout << "Total exits: " << exit_count << endl;
    }
};

void example1_basic() {
    cout << "\n=== Example 1: Basic Observer ===" << endl;
    
    SimpleObserver observer;
    
    parallel_for(blocked_range<int>(0, 100),
        [](blocked_range<int> r) {
            for(int i = r.begin(); i != r.end(); ++i) {
                volatile int x = i * i;  // Some work
            }
        });
    
    this_thread::sleep_for(milliseconds(100));  // Let threads finish
    observer.print_stats();
}

// ============= Example 2: Performance Monitoring =============

class PerformanceObserver : public task_scheduler_observer {
    struct ThreadInfo {
        chrono::high_resolution_clock::time_point start_time;
        long long total_time_us;
        int task_count;
        
        ThreadInfo() : total_time_us(0), task_count(0) {}
    };
    
    tbb::enumerable_thread_specific<ThreadInfo> thread_data;
    
public:
    PerformanceObserver() {
        observe(true);
    }
    
    void on_scheduler_entry(bool is_worker) override {
        auto& info = thread_data.local();
        info.start_time = high_resolution_clock::now();
    }
    
    void on_scheduler_exit(bool is_worker) override {
        auto& info = thread_data.local();
        auto elapsed = duration_cast<microseconds>(
            high_resolution_clock::now() - info.start_time).count();
        info.total_time_us += elapsed;
    }
    
    void print_stats() {
        cout << "\nPer-thread statistics:" << endl;
        int thread_num = 0;
        long long total_us = 0;
        
        for(auto& info : thread_data) {
            cout << "  Thread " << thread_num++ << ": "
                 << info.total_time_us << " μs" << endl;
            total_us += info.total_time_us;
        }
        
        cout << "Total time across all threads: " << total_us << " μs" << endl;
    }
};

void example2_performance() {
    cout << "\n=== Example 2: Performance Monitoring ===" << endl;
    
    PerformanceObserver observer;
    
    const int N = 1000000;
    vector<double> data(N, 1.0);
    
    parallel_for(blocked_range<int>(0, N),
        [&](blocked_range<int> r) {
            for(int i = r.begin(); i != r.end(); ++i) {
                data[i] = sqrt(data[i] + 1.0);
            }
        });
    
    this_thread::sleep_for(milliseconds(100));
    observer.print_stats();
}

// ============= Example 3: Thread Counting =============

class ThreadCounter : public task_scheduler_observer {
    std::atomic<int> active_threads;
    std::atomic<int> max_threads;
    std::atomic<int> total_threads;
    
public:
    ThreadCounter() : active_threads(0), max_threads(0), total_threads(0) {
        observe(true);
    }
    
    void on_scheduler_entry(bool is_worker) override {
        int current = ++active_threads;
        total_threads++;
        
        // Update max
        int expected = max_threads;
        while(current > expected &&
              !max_threads.compare_exchange_weak(expected, current)) {}
    }
    
    void on_scheduler_exit(bool is_worker) override {
        --active_threads;
    }
    
    void print_stats() {
        cout << "Active threads: " << active_threads << endl;
        cout << "Max concurrent threads: " << max_threads << endl;
        cout << "Total thread entries: " << total_threads << endl;
    }
};

void example3_counting() {
    cout << "\n=== Example 3: Thread Counting ===" << endl;
    
    ThreadCounter counter;
    
    parallel_for(blocked_range<int>(0, 10000),
        [](blocked_range<int> r) {
            for(int i = r.begin(); i != r.end(); ++i) {
                this_thread::sleep_for(microseconds(1));
            }
        });
    
    this_thread::sleep_for(milliseconds(100));
    counter.print_stats();
}

// ============= Example 4: Resource Management =============

class ResourceManager : public task_scheduler_observer {
    struct ThreadResources {
        vector<int> buffer;
        int allocations;
        
        ThreadResources() : buffer(1000, 0), allocations(0) {}
        ~ThreadResources() {}
    };
    
    enumerable_thread_specific<ThreadResources> resources;
    
public:
    ResourceManager() {
        observe(true);
    }
    
    void on_scheduler_entry(bool is_worker) override {
        auto& res = resources.local();
        res.allocations++;
        // Initialize thread-local resources
    }
    
    void on_scheduler_exit(bool is_worker) override {
        // Cleanup happens automatically via RAII
    }
    
    void print_stats() {
        cout << "Resources allocated per thread:" << endl;
        int thread_num = 0;
        for(auto& res : resources) {
            cout << "  Thread " << thread_num++ << ": "
                 << res.allocations << " allocations" << endl;
        }
    }
};

void example4_resources() {
    cout << "\n=== Example 4: Resource Management ===" << endl;
    
    ResourceManager manager;
    
    parallel_for(blocked_range<int>(0, 1000),
        [](blocked_range<int> r) {
            for(int i = r.begin(); i != r.end(); ++i) {
                vector<int> temp(100, i);
            }
        });
    
    this_thread::sleep_for(milliseconds(100));
    manager.print_stats();
}

// ============= Example 5: Debug Tracing =============

class DebugTracer : public task_scheduler_observer {
    tbb::spin_mutex mtx;
    
public:
    DebugTracer() {
        observe(true);
    }
    
    void on_scheduler_entry(bool is_worker) override {
        tbb::spin_mutex::scoped_lock lock(mtx);
        cout << "[TRACE] Thread " << this_thread::get_id()
             << " entered scheduler "
             << (is_worker ? "(worker)" : "(master)") << endl;
    }
    
    void on_scheduler_exit(bool is_worker) override {
        tbb::spin_mutex::scoped_lock lock(mtx);
        cout << "[TRACE] Thread " << this_thread::get_id()
             << " exited scheduler "
             << (is_worker ? "(worker)" : "(master)") << endl;
    }
};

void example5_tracing() {
    cout << "\n=== Example 5: Debug Tracing ===" << endl;
    
    DebugTracer tracer;
    
    parallel_for(blocked_range<int>(0, 20),
        [](blocked_range<int> r) {
            for(int i = r.begin(); i != r.end(); ++i) {
                this_thread::sleep_for(milliseconds(10));
            }
        });
    
    this_thread::sleep_for(milliseconds(100));
}

// ============= Example 6: Multiple Observers =============

class Observer1 : public task_scheduler_observer {
public:
    Observer1() { observe(true); }
    void on_scheduler_entry(bool) override {
        cout << "  Observer1: entry" << endl;
    }
    void on_scheduler_exit(bool) override {
        cout << "  Observer1: exit" << endl;
    }
};

class Observer2 : public task_scheduler_observer {
public:
    Observer2() { observe(true); }
    void on_scheduler_entry(bool) override {
        cout << "  Observer2: entry" << endl;
    }
    void on_scheduler_exit(bool) override {
        cout << "  Observer2: exit" << endl;
    }
};

void example6_multiple() {
    cout << "\n=== Example 6: Multiple Observers ===" << endl;
    
    Observer1 obs1;
    Observer2 obs2;
    
    cout << "Both observers active:" << endl;
    
    parallel_for(blocked_range<int>(0, 10),
        [](blocked_range<int> r) {
            for(int i = r.begin(); i != r.end(); ++i) {
                volatile int x = i;
            }
        });
    
    this_thread::sleep_for(milliseconds(100));
}

// ============= Example 7: Enable/Disable Observation =============

class ToggleObserver : public task_scheduler_observer {
    std::atomic<int> event_count;
    
public:
    ToggleObserver() : event_count(0) {
        // Don't observe initially
    }
    
    void on_scheduler_entry(bool) override {
        event_count++;
    }
    
    void enable() {
        cout << "Enabling observation" << endl;
        observe(true);
    }
    
    void disable() {
        cout << "Disabling observation" << endl;
        observe(false);
    }
    
    int get_count() const { return event_count; }
};

void example7_toggle() {
    cout << "\n=== Example 7: Enable/Disable Observation ===" << endl;
    
    ToggleObserver observer;
    
    cout << "Without observation:" << endl;
    parallel_for(blocked_range<int>(0, 100),
        [](blocked_range<int> r) {
            for(int i = r.begin(); i != r.end(); ++i) {
                volatile int x = i;
            }
        });
    cout << "Events: " << observer.get_count() << endl;
    
    observer.enable();
    
    cout << "\nWith observation:" << endl;
    parallel_for(blocked_range<int>(0, 100),
        [](blocked_range<int> r) {
            for(int i = r.begin(); i != r.end(); ++i) {
                volatile int x = i;
            }
        });
    
    this_thread::sleep_for(milliseconds(100));
    cout << "Events: " << observer.get_count() << endl;
}

int main() {
    cout << "╔════════════════════════════════════════════════════════╗" << endl;
    cout << "║  TBB task_scheduler_observer - Complete Tutorial      ║" << endl;
    cout << "╚════════════════════════════════════════════════════════╝" << endl;
    
    example1_basic();
    example2_performance();
    example3_counting();
    example4_resources();
    example5_tracing();
    example6_multiple();
    example7_toggle();
    
    cout << "\n╔════════════════════════════════════════════════════════╗" << endl;
    cout << "║                    KEY TAKEAWAYS                       ║" << endl;
    cout << "╠════════════════════════════════════════════════════════╣" << endl;
    cout << "║                                                        ║" << endl;
    cout << "║  1. Observe thread entry/exit in TBB scheduler        ║" << endl;
    cout << "║  2. on_scheduler_entry: thread joins scheduler        ║" << endl;
    cout << "║  3. on_scheduler_exit: thread leaves scheduler        ║" << endl;
    cout << "║  4. Use for thread-local initialization               ║" << endl;
    cout << "║  5. Great for performance monitoring                  ║" << endl;
    cout << "║  6. Can enable/disable with observe()                 ║" << endl;
    cout << "║  7. Multiple observers can coexist                    ║" << endl;
    cout << "║                                                        ║" << endl;
    cout << "╠════════════════════════════════════════════════════════╣" << endl;
    cout << "║               USAGE PATTERN                            ║" << endl;
    cout << "╠════════════════════════════════════════════════════════╣" << endl;
    cout << "║                                                        ║" << endl;
    cout << "║  class MyObserver : public task_scheduler_observer {  ║" << endl;
    cout << "║  public:                                               ║" << endl;
    cout << "║      MyObserver() {                                    ║" << endl;
    cout << "║          observe(true);  // Start observing            ║" << endl;
    cout << "║      }                                                 ║" << endl;
    cout << "║                                                        ║" << endl;
    cout << "║      void on_scheduler_entry(bool is_worker) override{║" << endl;
    cout << "║          // Thread joined scheduler                    ║" << endl;
    cout << "║      }                                                 ║" << endl;
    cout << "║                                                        ║" << endl;
    cout << "║      void on_scheduler_exit(bool is_worker) override {║" << endl;
    cout << "║          // Thread leaving scheduler                   ║" << endl;
    cout << "║      }                                                 ║" << endl;
    cout << "║  };                                                    ║" << endl;
    cout << "║                                                        ║" << endl;
    cout << "╠════════════════════════════════════════════════════════╣" << endl;
    cout << "║               WHEN TO USE                              ║" << endl;
    cout << "╠════════════════════════════════════════════════════════╣" << endl;
    cout << "║                                                        ║" << endl;
    cout << "║  ✓ Thread-local initialization/cleanup                ║" << endl;
    cout << "║  ✓ Performance profiling and monitoring               ║" << endl;
    cout << "║  ✓ CPU affinity/pinning                               ║" << endl;
    cout << "║  ✓ Custom thread naming                               ║" << endl;
    cout << "║  ✓ Resource management per thread                     ║" << endl;
    cout << "║  ✓ Debugging thread behavior                          ║" << endl;
    cout << "║  ✓ Integrating with other threading systems           ║" << endl;
    cout << "║  ✗ Regular parallel algorithms (use TBB directly)     ║" << endl;
    cout << "║                                                        ║" << endl;
    cout << "╚════════════════════════════════════════════════════════╝" << endl;
    
    return 0;
}
