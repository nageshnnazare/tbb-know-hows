/*
 * TBB CONCURRENT_BOUNDED_QUEUE - Bounded Queue with Blocking
 */

#include <iostream>
#include <cmath>
#include <thread>
#include <vector>
#include <chrono>
#include <tbb/concurrent_queue.h>
#include <tbb/parallel_for.h>

using namespace std;
using namespace tbb;
using namespace chrono;

void example1_basic() {
    cout << "\n=== Example 1: Bounded Queue ===" << endl;
    
    concurrent_bounded_queue<int> queue;
    queue.set_capacity(5);
    
    cout << "Queue capacity: " << queue.capacity() << endl;
    
    for(int i = 0; i < 5; ++i) {
        queue.push(i);
        cout << "Pushed: " << i << endl;
    }
    
    cout << "Queue is full (size=" << queue.size() << ")" << endl;
    
    int item;
    while(queue.try_pop(item)) {
        cout << "Popped: " << item << endl;
    }
}

void example2_blocking() {
    cout << "\n=== Example 2: Blocking Operations ===" << endl;
    
    concurrent_bounded_queue<int> queue;
    queue.set_capacity(10);
    
    thread producer([&]() {
        for(int i = 0; i < 20; ++i) {
            queue.push(i);  // Blocks when full
            cout << "Produced: " << i << endl;
            this_thread::sleep_for(milliseconds(10));
        }
    });
    
    this_thread::sleep_for(milliseconds(50));
    
    thread consumer([&]() {
        for(int i = 0; i < 20; ++i) {
            int item;
            queue.pop(item);  // Blocks when empty
            cout << "Consumed: " << item << endl;
            this_thread::sleep_for(milliseconds(15));
        }
    });
    
    producer.join();
    consumer.join();
    
    cout << "Blocking queue ensures backpressure!" << endl;
}

int main() {
    cout << "╔════════════════════════════════════════════════════════╗" << endl;
    cout << "║  TBB concurrent_bounded_queue - Complete Tutorial     ║" << endl;
    cout << "╚════════════════════════════════════════════════════════╝" << endl;
    
    example1_basic();
    example2_blocking();
    
    cout << "\n╔════════════════════════════════════════════════════════╗" << endl;
    cout << "║                    KEY TAKEAWAYS                       ║" << endl;
    cout << "╠════════════════════════════════════════════════════════╣" << endl;
    cout << "║  1. Bounded capacity with blocking                    ║" << endl;
    cout << "║  2. push() blocks when full                           ║" << endl;
    cout << "║  3. pop() blocks when empty                           ║" << endl;
    cout << "║  4. Prevents memory overflow                          ║" << endl;
    cout << "║  5. Backpressure mechanism                            ║" << endl;
    cout << "╚════════════════════════════════════════════════════════╝" << endl;
    
    return 0;
}
