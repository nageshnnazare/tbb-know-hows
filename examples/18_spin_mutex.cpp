/*
 * TBB SPIN_MUTEX - Spinlock for Short Critical Sections
 */

#include <iostream>
#include <cmath>
#include <vector>
#include <chrono>
#include <tbb/spin_mutex.h>
#include <tbb/parallel_for.h>
#include <tbb/blocked_range.h>

using namespace std;
using namespace tbb;
using namespace chrono;

void example1_basic() {
    cout << "\n=== Example 1: Basic spin_mutex ===" << endl;
    
    spin_mutex mtx;
    int counter = 0;
    
    parallel_for(blocked_range<int>(0, 10000),
        [&](blocked_range<int> r) {
            for(int i = r.begin(); i != r.end(); ++i) {
                spin_mutex::scoped_lock lock(mtx);
                counter++;  // Very short critical section
            }
        });
    
    cout << "Counter: " << counter << endl;
}

void example2_performance() {
    cout << "\n=== Example 2: Best for Short Locks ===" << endl;
    
    const int N = 100000;
    
    spin_mutex spin_mtx;
    int spin_counter = 0;
    
    auto start = high_resolution_clock::now();
    parallel_for(blocked_range<int>(0, N),
        [&](blocked_range<int> r) {
            for(int i = r.begin(); i != r.end(); ++i) {
                spin_mutex::scoped_lock lock(spin_mtx);
                spin_counter++;  // Tiny critical section
            }
        });
    auto spin_time = duration_cast<milliseconds>(
        high_resolution_clock::now() - start).count();
    
    cout << "spin_mutex: " << spin_time << " ms" << endl;
    cout << "Best for critical sections < 1 microsecond" << endl;
}

int main() {
    cout << "╔════════════════════════════════════════════════════════╗" << endl;
    cout << "║      TBB spin_mutex - Complete Tutorial               ║" << endl;
    cout << "╚════════════════════════════════════════════════════════╝" << endl;
    
    example1_basic();
    example2_performance();
    
    cout << "\n╔════════════════════════════════════════════════════════╗" << endl;
    cout << "║                    KEY TAKEAWAYS                       ║" << endl;
    cout << "╠════════════════════════════════════════════════════════╣" << endl;
    cout << "║  1. Busy-wait spinlock (no context switch)            ║" << endl;
    cout << "║  2. Best for very short critical sections             ║" << endl;
    cout << "║  3. Wastes CPU while waiting                          ║" << endl;
    cout << "║  4. Lower overhead than regular mutex                 ║" << endl;
    cout << "║  5. Use for sub-microsecond lock times                ║" << endl;
    cout << "╚════════════════════════════════════════════════════════╝" << endl;
    
    return 0;
}
