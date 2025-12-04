/*
 * TBB CONCURRENT_PRIORITY_QUEUE - Thread-Safe Priority Queue
 */

#include <iostream>
#include <cmath>
#include <vector>
#include <chrono>
#include <tbb/concurrent_priority_queue.h>
#include <tbb/parallel_for.h>

using namespace std;
using namespace tbb;
using namespace chrono;

void example1_basic() {
    cout << "\n=== Example 1: Basic Priority Queue ===" << endl;
    
    concurrent_priority_queue<int> pq;
    
    pq.push(5);
    pq.push(1);
    pq.push(10);
    pq.push(3);
    pq.push(7);
    
    cout << "Pushed: 5, 1, 10, 3, 7" << endl;
    cout << "Popping (highest first): ";
    
    int val;
    while(pq.try_pop(val)) {
        cout << val << " ";
    }
    cout << endl;
}

void example2_concurrent() {
    cout << "\n=== Example 2: Concurrent Operations ===" << endl;
    
    concurrent_priority_queue<int> pq;
    
    parallel_for(blocked_range<int>(0, 1000),
        [&](blocked_range<int> r) {
            for(int i = r.begin(); i != r.end(); ++i) {
                pq.push(i);
            }
        });
    
    cout << "Pushed 1000 items concurrently" << endl;
    cout << "Size: " << pq.size() << endl;
    
    int val;
    cout << "Top 10: ";
    for(int i = 0; i < 10 && pq.try_pop(val); ++i) {
        cout << val << " ";
    }
    cout << endl;
}

void example3_custom_comparator() {
    cout << "\n=== Example 3: Custom Comparator ===" << endl;
    
    concurrent_priority_queue<int, less<int>> min_heap;  // Min at top
    
    min_heap.push(5);
    min_heap.push(1);
    min_heap.push(10);
    
    int val;
    cout << "Min heap (smallest first): ";
    while(min_heap.try_pop(val)) {
        cout << val << " ";
    }
    cout << endl;
}

struct Task {
    int priority;
    string description;
    
    bool operator<(const Task& other) const {
        return priority < other.priority;
    }
};

void example4_task_scheduling() {
    cout << "\n=== Example 4: Priority Task Scheduling ===" << endl;
    
    concurrent_priority_queue<Task> task_queue;
    
    task_queue.push({10, "High priority"});
    task_queue.push({5, "Medium priority"});
    task_queue.push({15, "Critical"});
    task_queue.push({3, "Low priority"});
    
    cout << "Tasks (processed by priority):" << endl;
    Task task;
    while(task_queue.try_pop(task)) {
        cout << "  [" << task.priority << "] " << task.description << endl;
    }
}

int main() {
    cout << "╔════════════════════════════════════════════════════════╗" << endl;
    cout << "║  TBB concurrent_priority_queue - Complete Tutorial    ║" << endl;
    cout << "╚════════════════════════════════════════════════════════╝" << endl;
    
    example1_basic();
    example2_concurrent();
    example3_custom_comparator();
    example4_task_scheduling();
    
    cout << "\n╔════════════════════════════════════════════════════════╗" << endl;
    cout << "║                    KEY TAKEAWAYS                       ║" << endl;
    cout << "╠════════════════════════════════════════════════════════╣" << endl;
    cout << "║  1. Thread-safe priority queue                        ║" << endl;
    cout << "║  2. Highest priority element popped first             ║" << endl;
    cout << "║  3. Concurrent push and try_pop                       ║" << endl;
    cout << "║  4. Custom comparators supported                      ║" << endl;
    cout << "║  5. No iterators (like std::priority_queue)           ║" << endl;
    cout << "║  6. Perfect for priority task scheduling              ║" << endl;
    cout << "╚════════════════════════════════════════════════════════╝" << endl;
    
    return 0;
}
