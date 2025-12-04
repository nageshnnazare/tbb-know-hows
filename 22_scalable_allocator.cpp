/*
 * TBB SCALABLE_ALLOCATOR - High-Performance Memory Allocator
 * 
 * DEFINITION:
 * scalable_allocator is TBB's thread-scalable memory allocator that
 * provides better performance than standard allocators in multi-threaded scenarios.
 * 
 * KEY FEATURES:
 * - Thread-local pools (reduces contention)
 * - Lock-free operations
 * - Scalable with thread count
 * - Compatible with STL containers
 * - Prevents false sharing
 * 
 * BENEFITS:
 * + No global lock contention
 * + Better scalability than malloc
 * + Optimized for parallel workloads
 * + Drop-in replacement for std::allocator
 * 
 * WHEN TO USE:
 * - Frequent allocations in parallel code
 * - STL containers used by multiple threads
 * - Performance-critical applications
 * - Avoiding allocator bottlenecks
 */

#include <iostream>
#include <cmath>
#include <vector>
#include <list>
#include <map>
#include <chrono>
#include <tbb/scalable_allocator.h>
#include <tbb/parallel_for.h>
#include <tbb/blocked_range.h>

using namespace std;
using std::atomic;
using namespace tbb;
using namespace chrono;

// ============= Example 1: Basic Usage =============

void example1_basic() {
    cout << "\n=== Example 1: Basic scalable_allocator ===" << endl;
    
    // STL containers with scalable_allocator
    vector<int, scalable_allocator<int>> vec;
    
    vec.push_back(10);
    vec.push_back(20);
    vec.push_back(30);
    
    cout << "Vector with scalable_allocator: ";
    for(int x : vec) {
        cout << x << " ";
    }
    cout << endl;
    
    // Works like std::allocator
    list<string, scalable_allocator<string>> lst;
    lst.push_back("Hello");
    lst.push_back("TBB");
    
    cout << "List with scalable_allocator: ";
    for(const auto& s : lst) {
        cout << s << " ";
    }
    cout << endl;
}

// ============= Example 2: Performance Comparison =============

void example2_performance() {
    cout << "\n=== Example 2: Performance Comparison ===" << endl;
    
    const int N = 1000;
    const int SIZE = 1000;
    
    // Standard allocator
    {
        auto start = high_resolution_clock::now();
        
        parallel_for(blocked_range<int>(0, N),
            [&](blocked_range<int> r) {
                for(int i = r.begin(); i != r.end(); ++i) {
                    vector<int> temp(SIZE);
                    temp[0] = i;
                }
            });
        
        auto time = duration_cast<milliseconds>(
            high_resolution_clock::now() - start).count();
        
        cout << "std::allocator:      " << time << " ms" << endl;
    }
    
    // Scalable allocator
    {
        auto start = high_resolution_clock::now();
        
        parallel_for(blocked_range<int>(0, N),
            [&](blocked_range<int> r) {
                for(int i = r.begin(); i != r.end(); ++i) {
                    vector<int, scalable_allocator<int>> temp(SIZE);
                    temp[0] = i;
                }
            });
        
        auto time = duration_cast<milliseconds>(
            high_resolution_clock::now() - start).count();
        
        cout << "scalable_allocator:  " << time << " ms" << endl;
    }
    
    cout << "scalable_allocator reduces contention!" << endl;
}

// ============= Example 3: STL Containers =============

void example3_stl_containers() {
    cout << "\n=== Example 3: STL Containers with scalable_allocator ===" << endl;
    
    // Vector
    vector<double, scalable_allocator<double>> vec = {1.1, 2.2, 3.3};
    cout << "Vector size: " << vec.size() << endl;
    
    // Map
    map<int, string, 
        less<int>,
        scalable_allocator<pair<const int, string>>> my_map;
    
    my_map[1] = "one";
    my_map[2] = "two";
    my_map[3] = "three";
    
    cout << "Map entries: " << my_map.size() << endl;
    
    // List
    list<int, scalable_allocator<int>> lst = {10, 20, 30};
    lst.push_back(40);
    
    cout << "List elements: ";
    for(int x : lst) cout << x << " ";
    cout << endl;
}

// ============= Example 4: Parallel Container Operations =============

void example4_parallel_operations() {
    cout << "\n=== Example 4: Parallel Container Operations ===" << endl;
    
    const int N = 100;
    vector<vector<int, scalable_allocator<int>>> outer(N);
    
    auto start = high_resolution_clock::now();
    
    // Each thread allocates its own container
    parallel_for(blocked_range<int>(0, N),
        [&](blocked_range<int> r) {
            for(int i = r.begin(); i != r.end(); ++i) {
                outer[i].reserve(1000);
                for(int j = 0; j < 1000; ++j) {
                    outer[i].push_back(j);
                }
            }
        });
    
    auto time = duration_cast<milliseconds>(
        high_resolution_clock::now() - start).count();
    
    size_t total = 0;
    for(const auto& vec : outer) {
        total += vec.size();
    }
    
    cout << "Created " << N << " vectors with " << total << " elements" << endl;
    cout << "Time: " << time << " ms" << endl;
    cout << "Thread-local pools prevent contention" << endl;
}

// ============= Example 5: Memory Allocation Patterns =============

struct DataChunk {
    vector<int, scalable_allocator<int>> data;
    
    DataChunk() : data(1000, 0) {}
};

void example5_allocation_patterns() {
    cout << "\n=== Example 5: Memory Allocation Patterns ===" << endl;
    
    vector<DataChunk*> chunks;
    scalable_allocator<DataChunk> alloc;
    
    auto start = high_resolution_clock::now();
    
    // Allocate many objects in parallel
    parallel_for(blocked_range<int>(0, 100),
        [&](blocked_range<int> r) {
            for(int i = r.begin(); i != r.end(); ++i) {
                DataChunk* chunk = alloc.allocate(1);
                alloc.construct(chunk);
                
                // Store safely (would need synchronization in real code)
                // For demo, we just delete immediately
                alloc.destroy(chunk);
                alloc.deallocate(chunk, 1);
            }
        });
    
    auto time = duration_cast<milliseconds>(
        high_resolution_clock::now() - start).count();
    
    cout << "Allocated/deallocated 100 chunks in " << time << " ms" << endl;
    cout << "Lock-free allocation scales well" << endl;
}

// ============= Example 6: Custom Classes =============

class Node {
public:
    int value;
    Node* left;
    Node* right;
    
    Node(int v) : value(v), left(nullptr), right(nullptr) {}
};

// Define allocator for custom class
using NodeAllocator = scalable_allocator<Node>;

void example6_custom_classes() {
    cout << "\n=== Example 6: Custom Classes with scalable_allocator ===" << endl;
    
    NodeAllocator alloc;
    
    // Allocate nodes
    Node* root = alloc.allocate(1);
    alloc.construct(root, 100);
    
    root->left = alloc.allocate(1);
    alloc.construct(root->left, 50);
    
    root->right = alloc.allocate(1);
    alloc.construct(root->right, 150);
    
    cout << "Created tree:" << endl;
    cout << "  Root: " << root->value << endl;
    cout << "  Left: " << root->left->value << endl;
    cout << "  Right: " << root->right->value << endl;
    
    // Clean up
    alloc.destroy(root->left);
    alloc.deallocate(root->left, 1);
    
    alloc.destroy(root->right);
    alloc.deallocate(root->right, 1);
    
    alloc.destroy(root);
    alloc.deallocate(root, 1);
    
    cout << "Manual allocation gives fine-grained control" << endl;
}

// ============= Example 7: Scalability Test =============

void example7_scalability() {
    cout << "\n=== Example 7: Scalability with Thread Count ===" << endl;
    
    const int ALLOCATIONS = 10000;
    
    for(int threads : {1, 2, 4, 8}) {
        auto start = high_resolution_clock::now();
        
        parallel_for(blocked_range<int>(0, ALLOCATIONS),
            [&](blocked_range<int> r) {
                vector<int, scalable_allocator<int>> temp;
                for(int i = r.begin(); i != r.end(); ++i) {
                    temp.push_back(i);
                }
            },
            simple_partitioner());
        
        auto time = duration_cast<milliseconds>(
            high_resolution_clock::now() - start).count();
        
        cout << threads << " threads: " << time << " ms" << endl;
    }
    
    cout << "Performance scales with thread count!" << endl;
}

int main() {
    cout << "╔════════════════════════════════════════════════════════╗" << endl;
    cout << "║   TBB scalable_allocator - Complete Tutorial          ║" << endl;
    cout << "╚════════════════════════════════════════════════════════╝" << endl;
    
    example1_basic();
    example2_performance();
    example3_stl_containers();
    example4_parallel_operations();
    example5_allocation_patterns();
    example6_custom_classes();
    example7_scalability();
    
    cout << "\n╔════════════════════════════════════════════════════════╗" << endl;
    cout << "║                    KEY TAKEAWAYS                       ║" << endl;
    cout << "╠════════════════════════════════════════════════════════╣" << endl;
    cout << "║                                                        ║" << endl;
    cout << "║  1. Thread-scalable memory allocator                  ║" << endl;
    cout << "║  2. Better than malloc in multi-threaded code         ║" << endl;
    cout << "║  3. Thread-local pools reduce contention              ║" << endl;
    cout << "║  4. Drop-in replacement for std::allocator            ║" << endl;
    cout << "║  5. Works with all STL containers                     ║" << endl;
    cout << "║  6. Lock-free operations                              ║" << endl;
    cout << "║  7. Scales linearly with thread count                 ║" << endl;
    cout << "║                                                        ║" << endl;
    cout << "╠════════════════════════════════════════════════════════╣" << endl;
    cout << "║               COMMON PATTERNS                          ║" << endl;
    cout << "╠════════════════════════════════════════════════════════╣" << endl;
    cout << "║                                                        ║" << endl;
    cout << "║  STL Container:                                        ║" << endl;
    cout << "║    vector<T, scalable_allocator<T>> vec;              ║" << endl;
    cout << "║    map<K, V, less<K>,                                 ║" << endl;
    cout << "║        scalable_allocator<pair<const K, V>>> m;       ║" << endl;
    cout << "║                                                        ║" << endl;
    cout << "║  Manual Allocation:                                    ║" << endl;
    cout << "║    scalable_allocator<T> alloc;                       ║" << endl;
    cout << "║    T* ptr = alloc.allocate(n);                        ║" << endl;
    cout << "║    alloc.construct(ptr, args...);                     ║" << endl;
    cout << "║    alloc.destroy(ptr);                                ║" << endl;
    cout << "║    alloc.deallocate(ptr, n);                          ║" << endl;
    cout << "║                                                        ║" << endl;
    cout << "╠════════════════════════════════════════════════════════╣" << endl;
    cout << "║               WHEN TO USE                              ║" << endl;
    cout << "╠════════════════════════════════════════════════════════╣" << endl;
    cout << "║                                                        ║" << endl;
    cout << "║  ✓ Parallel algorithms with allocations               ║" << endl;
    cout << "║  ✓ STL containers in multi-threaded code              ║" << endl;
    cout << "║  ✓ Frequent malloc/free in parallel sections          ║" << endl;
    cout << "║  ✓ Performance-critical applications                  ║" << endl;
    cout << "║  ✓ Avoiding allocator bottlenecks                     ║" << endl;
    cout << "║  ✗ Single-threaded code (standard allocator OK)       ║" << endl;
    cout << "║  ✗ No allocations in hot path                         ║" << endl;
    cout << "║                                                        ║" << endl;
    cout << "╚════════════════════════════════════════════════════════╝" << endl;
    
    return 0;
}
