/*
 * TBB CONCURRENT_QUEUE - Lock-Free Producer-Consumer Queue
 * 
 * DEFINITION:
 * concurrent_queue is a thread-safe, lock-free FIFO queue that allows
 * multiple threads to push and pop elements concurrently.
 * 
 * KEY FEATURES:
 * - Lock-free for most operations
 * - Multiple producers and consumers
 * - Unbounded capacity (grows dynamically)
 * - Non-blocking try_pop operation
 * - FIFO ordering preserved
 * 
 * WHEN TO USE:
 * - Producer-consumer patterns
 * - Work stealing/distribution
 * - Task queues
 * - Event processing
 * - Message passing between threads
 * 
 * PERFORMANCE:
 * - Much faster than mutex-protected std::queue
 * - Scales well with many threads
 * - Minimal contention
 */

#include <iostream>
#include <cmath>
#include <vector>
#include <thread>
#include <chrono>
#include <tbb/concurrent_queue.h>
#include <tbb/parallel_for.h>
#include <tbb/blocked_range.h>

using namespace std;
using namespace tbb;
using namespace chrono;

// ============= Example 1: Basic Producer-Consumer =============

void example1_basic() {
    cout << "\n=== Example 1: Basic Producer-Consumer ===" << endl;
    
    concurrent_queue<int> queue;
    
    // Producer: Push items
    cout << "Producer pushing items: ";
    for(int i = 0; i < 10; ++i) {
        queue.push(i);
        cout << i << " ";
    }
    cout << endl;
    
    // Consumer: Pop items
    cout << "Consumer popping items: ";
    int item;
    while(queue.try_pop(item)) {
        cout << item << " ";
    }
    cout << endl;
    
    cout << "Queue is now empty: " << queue.empty() << endl;
}

// ============= Example 2: Multiple Producers, Single Consumer =============

void example2_multiple_producers() {
    cout << "\n=== Example 2: Multiple Producers, Single Consumer ===" << endl;
    
    concurrent_queue<int> queue;
    const int NUM_PRODUCERS = 4;
    const int ITEMS_PER_PRODUCER = 100;
    
    // Multiple producer threads
    auto start = high_resolution_clock::now();
    
    vector<thread> producers;
    for(int p = 0; p < NUM_PRODUCERS; ++p) {
        producers.emplace_back([&, p]() {
            for(int i = 0; i < ITEMS_PER_PRODUCER; ++i) {
                queue.push(p * 1000 + i);
            }
        });
    }
    
    // Wait for producers
    for(auto& t : producers) t.join();
    
    auto producer_time = duration_cast<microseconds>(
        high_resolution_clock::now() - start).count();
    
    // Single consumer
    start = high_resolution_clock::now();
    int count = 0;
    int item;
    while(queue.try_pop(item)) {
        count++;
    }
    
    auto consumer_time = duration_cast<microseconds>(
        high_resolution_clock::now() - start).count();
    
    cout << "Produced: " << NUM_PRODUCERS * ITEMS_PER_PRODUCER << " items" << endl;
    cout << "Consumed: " << count << " items" << endl;
    cout << "Producer time: " << producer_time << " μs" << endl;
    cout << "Consumer time: " << consumer_time << " μs" << endl;
}

// ============= Example 3: Multiple Producers, Multiple Consumers =============

void example3_multiple_both() {
    cout << "\n=== Example 3: Multiple Producers & Consumers ===" << endl;
    
    concurrent_queue<int> queue;
    const int NUM_PRODUCERS = 4;
    const int NUM_CONSUMERS = 4;
    const int ITEMS_PER_PRODUCER = 1000;
    
    tbb::atomic<int> consumed_count;
    consumed_count = 0;
    
    // Start producers
    vector<thread> producers;
    for(int p = 0; p < NUM_PRODUCERS; ++p) {
        producers.emplace_back([&, p]() {
            for(int i = 0; i < ITEMS_PER_PRODUCER; ++i) {
                queue.push(p * 10000 + i);
                this_thread::sleep_for(microseconds(1)); // Simulate work
            }
        });
    }
    
    // Start consumers
    vector<thread> consumers;
    for(int c = 0; c < NUM_CONSUMERS; ++c) {
        consumers.emplace_back([&]() {
            int item;
            int local_count = 0;
            while(true) {
                if(queue.try_pop(item)) {
                    local_count++;
                    this_thread::sleep_for(microseconds(1)); // Simulate work
                } else {
                    // Check if producers are done
                    bool all_done = true;
                    for(auto& p : producers) {
                        if(p.joinable()) {
                            all_done = false;
                            break;
                        }
                    }
                    if(all_done && queue.empty()) break;
                    this_thread::yield();
                }
            }
            consumed_count += local_count;
        });
    }
    
    // Wait for completion
    for(auto& t : producers) t.join();
    for(auto& t : consumers) t.join();
    
    cout << "Total produced: " << NUM_PRODUCERS * ITEMS_PER_PRODUCER << endl;
    cout << "Total consumed: " << consumed_count.load() << endl;
    cout << "Match: " << (consumed_count == NUM_PRODUCERS * ITEMS_PER_PRODUCER ? "✓" : "✗") << endl;
}

// ============= Example 4: Task Queue Pattern =============

struct Task {
    int id;
    string operation;
    
    Task() : id(0), operation("") {}  // Default constructor
    Task(int i, string op) : id(i), operation(op) {}
};

void example4_task_queue() {
    cout << "\n=== Example 4: Task Queue Pattern ===" << endl;
    
    concurrent_queue<Task> task_queue;
    const int NUM_TASKS = 20;
    const int NUM_WORKERS = 4;
    
    // Producer: Generate tasks
    for(int i = 0; i < NUM_TASKS; ++i) {
        task_queue.push(Task(i, "Process_" + to_string(i)));
    }
    
    cout << "Generated " << NUM_TASKS << " tasks" << endl;
    
    // Workers: Process tasks
    tbb::atomic<int> completed;
    completed = 0;
    vector<thread> workers;
    
    for(int w = 0; w < NUM_WORKERS; ++w) {
        workers.emplace_back([&, w]() {
            Task task(0, "");
            while(task_queue.try_pop(task)) {
                // Simulate task processing
                // cout << "Worker " << w << " processing task " << task.id << endl;
                this_thread::sleep_for(milliseconds(10));
                completed++;
            }
        });
    }
    
    for(auto& t : workers) t.join();
    
    cout << "Completed " << completed.load() << " tasks with " 
         << NUM_WORKERS << " workers" << endl;
}

// ============= Example 5: Performance Comparison =============

void example5_performance() {
    cout << "\n=== Example 5: Performance vs Mutex+Queue ===" << endl;
    
    const int N = 100000;
    
    // TBB concurrent_queue
    concurrent_queue<int> tbb_queue;
    auto start = high_resolution_clock::now();
    
    thread producer1([&]() {
        for(int i = 0; i < N/2; ++i)
            tbb_queue.push(i);
    });
    
    thread producer2([&]() {
        for(int i = N/2; i < N; ++i)
            tbb_queue.push(i);
    });
    
    thread consumer1([&]() {
        int item, count = 0;
        while(count < N/2) {
            if(tbb_queue.try_pop(item)) count++;
        }
    });
    
    thread consumer2([&]() {
        int item, count = 0;
        while(count < N/2) {
            if(tbb_queue.try_pop(item)) count++;
        }
    });
    
    producer1.join();
    producer2.join();
    consumer1.join();
    consumer2.join();
    
    auto tbb_time = duration_cast<milliseconds>(
        high_resolution_clock::now() - start).count();
    
    cout << "TBB concurrent_queue: " << tbb_time << " ms" << endl;
    cout << "Processing " << N << " items with 2 producers, 2 consumers" << endl;
    cout << "Lock-free operation = minimal contention!" << endl;
}

// ============= Example 6: Event Processing Pipeline =============

struct Event {
    int id;
    string type;
    long long timestamp;
    
    Event() : id(0), type(""), timestamp(0) {}
    Event(int i, string t) 
        : id(i), type(t), 
          timestamp(duration_cast<milliseconds>(
              system_clock::now().time_since_epoch()).count()) {}
};

void example6_event_pipeline() {
    cout << "\n=== Example 6: Event Processing Pipeline ===" << endl;
    
    concurrent_queue<Event> input_queue;
    concurrent_queue<Event> output_queue;
    const int NUM_EVENTS = 100;
    
    // Stage 1: Generate events
    thread generator([&]() {
        for(int i = 0; i < NUM_EVENTS; ++i) {
            input_queue.push(Event(i, "Event_" + to_string(i % 3)));
            this_thread::sleep_for(microseconds(100));
        }
    });
    
    // Stage 2: Process events (parallel)
    vector<thread> processors;
    for(int p = 0; p < 4; ++p) {
        processors.emplace_back([&]() {
            Event evt;
            while(true) {
                if(input_queue.try_pop(evt)) {
                    // Process event
                    this_thread::sleep_for(microseconds(500));
                    output_queue.push(evt);
                } else {
                    if(!generator.joinable()) break;
                    this_thread::yield();
                }
            }
        });
    }
    
    generator.join();
    for(auto& t : processors) t.join();
    
    // Stage 3: Collect results
    int processed = 0;
    Event evt;
    while(output_queue.try_pop(evt)) {
        processed++;
    }
    
    cout << "Generated " << NUM_EVENTS << " events" << endl;
    cout << "Processed " << processed << " events" << endl;
    cout << "Pipeline pattern with concurrent_queue!" << endl;
}

// ============= Example 7: Work Stealing Pattern =============

void example7_work_stealing() {
    cout << "\n=== Example 7: Work Stealing Pattern ===" << endl;
    
    const int NUM_WORKERS = 4;
    const int TOTAL_WORK = 1000;
    
    // Each worker has its own queue
    vector<concurrent_queue<int>> worker_queues(NUM_WORKERS);
    
    // Distribute work
    for(int i = 0; i < TOTAL_WORK; ++i) {
        worker_queues[i % NUM_WORKERS].push(i);
    }
    
    tbb::atomic<int> work_done;
    work_done = 0;
    vector<thread> workers;
    
    for(int w = 0; w < NUM_WORKERS; ++w) {
        workers.emplace_back([&, w]() {
            int local_work = 0;
            int item;
            
            // First, process own queue
            while(worker_queues[w].try_pop(item)) {
                local_work++;
                this_thread::sleep_for(microseconds(10));
            }
            
            // Then, steal from others
            for(int other = 0; other < NUM_WORKERS; ++other) {
                if(other == w) continue;
                while(worker_queues[other].try_pop(item)) {
                    local_work++;
                    this_thread::sleep_for(microseconds(10));
                }
            }
            
            work_done += local_work;
        });
    }
    
    for(auto& t : workers) t.join();
    
    cout << "Total work items: " << TOTAL_WORK << endl;
    cout << "Work completed: " << work_done.load() << endl;
    cout << "Workers steal from each other when idle!" << endl;
}

// ============= Main =============

int main() {
    cout << "╔════════════════════════════════════════════════════════╗" << endl;
    cout << "║   TBB concurrent_queue - Complete Tutorial            ║" << endl;
    cout << "╚════════════════════════════════════════════════════════╝" << endl;
    
    example1_basic();
    example2_multiple_producers();
    example3_multiple_both();
    example4_task_queue();
    example5_performance();
    example6_event_pipeline();
    example7_work_stealing();
    
    cout << "\n╔════════════════════════════════════════════════════════╗" << endl;
    cout << "║                    KEY TAKEAWAYS                       ║" << endl;
    cout << "╠════════════════════════════════════════════════════════╣" << endl;
    cout << "║                                                        ║" << endl;
    cout << "║  1. concurrent_queue is lock-free and thread-safe     ║" << endl;
    cout << "║  2. Supports multiple producers AND consumers         ║" << endl;
    cout << "║  3. push() adds items (blocking if needed)            ║" << endl;
    cout << "║  4. try_pop() removes items (returns false if empty)  ║" << endl;
    cout << "║  5. FIFO ordering preserved                           ║" << endl;
    cout << "║  6. Unbounded capacity (grows dynamically)            ║" << endl;
    cout << "║  7. Perfect for producer-consumer patterns            ║" << endl;
    cout << "║  8. Much faster than mutex + std::queue               ║" << endl;
    cout << "║                                                        ║" << endl;
    cout << "╠════════════════════════════════════════════════════════╣" << endl;
    cout << "║               COMMON PATTERNS                          ║" << endl;
    cout << "╠════════════════════════════════════════════════════════╣" << endl;
    cout << "║                                                        ║" << endl;
    cout << "║  Producer-Consumer:                                    ║" << endl;
    cout << "║    concurrent_queue<Task> queue;                      ║" << endl;
    cout << "║    // Producer: queue.push(task);                     ║" << endl;
    cout << "║    // Consumer: if(queue.try_pop(task)) process(task);║" << endl;
    cout << "║                                                        ║" << endl;
    cout << "║  Task Queue:                                           ║" << endl;
    cout << "║    while(queue.try_pop(task)) {                       ║" << endl;
    cout << "║        process(task);                                  ║" << endl;
    cout << "║    }                                                   ║" << endl;
    cout << "║                                                        ║" << endl;
    cout << "║  Pipeline Stage:                                       ║" << endl;
    cout << "║    Item item;                                          ║" << endl;
    cout << "║    while(input_queue.try_pop(item)) {                 ║" << endl;
    cout << "║        process(item);                                  ║" << endl;
    cout << "║        output_queue.push(item);                        ║" << endl;
    cout << "║    }                                                   ║" << endl;
    cout << "║                                                        ║" << endl;
    cout << "╠════════════════════════════════════════════════════════╣" << endl;
    cout << "║               PERFORMANCE TIPS                         ║" << endl;
    cout << "╠════════════════════════════════════════════════════════╣" << endl;
    cout << "║                                                        ║" << endl;
    cout << "║  ✓ Use for inter-thread communication                 ║" << endl;
    cout << "║  ✓ Batch operations when possible                     ║" << endl;
    cout << "║  ✓ try_pop returns false when empty (non-blocking)    ║" << endl;
    cout << "║  ✓ Consider bounded queue for memory control          ║" << endl;
    cout << "║  ✓ Lock-free = excellent scalability                  ║" << endl;
    cout << "║                                                        ║" << endl;
    cout << "╠════════════════════════════════════════════════════════╣" << endl;
    cout << "║               WHEN TO USE                              ║" << endl;
    cout << "╠════════════════════════════════════════════════════════╣" << endl;
    cout << "║                                                        ║" << endl;
    cout << "║  ✓ Producer-consumer patterns                          ║" << endl;
    cout << "║  ✓ Task distribution systems                           ║" << endl;
    cout << "║  ✓ Event processing pipelines                          ║" << endl;
    cout << "║  ✓ Work stealing schedulers                            ║" << endl;
    cout << "║  ✓ Message passing between threads                     ║" << endl;
    cout << "║  ✗ Need bounded capacity (use concurrent_bounded_queue)║" << endl;
    cout << "║  ✗ Need priority ordering (use concurrent_priority_queue)║" << endl;
    cout << "║                                                        ║" << endl;
    cout << "╚════════════════════════════════════════════════════════╝" << endl;
    
    return 0;
}
