/*
 * TBB PARALLEL_PIPELINE - Streaming Data Processing
 * 
 * DEFINITION:
 * parallel_pipeline processes items through multiple stages,
 * where each stage can process different items concurrently.
 * 
 * USE CASES:
 * - Video/audio processing
 * - Data transformation pipelines
 * - Stream processing
 * - Producer-consumer with multiple stages
 */

#include <iostream>
#include <vector>
#include <tbb/pipeline.h>
#include <chrono>
#include <cmath>

using namespace std;
using namespace tbb;
using namespace chrono;

// ============= Example 1: Simple 3-Stage Pipeline =============

void example1_basic() {
    cout << "\n=== Example 1: Basic 3-Stage Pipeline ===" << endl;
    
    const int N = 100;
    int processed_count = 0;
    
    parallel_pipeline(
        4,  // max_tokens (max items in flight)
        
        // Stage 1: Generate items (serial)
        make_filter<void, int>(
            filter::serial_in_order,
            [&](flow_control& fc) -> int {
                if(processed_count < N) {
                    return processed_count++;
                } else {
                    fc.stop();
                    return 0;
                }
            }
        )
        &
        // Stage 2: Process items (parallel)
        make_filter<int, int>(
            filter::parallel,
            [](int item) -> int {
                return item * item;  // Square
            }
        )
        &
        // Stage 3: Output items (serial)
        make_filter<int, void>(
            filter::serial_in_order,
            [](int result) {
                if(result < 10)
                    cout << result << " ";
            }
        )
    );
    
    cout << "\nProcessed " << N << " items through pipeline" << endl;
}

// ============= Example 2: Image Processing Pipeline =============

struct Image {
    int id;
    vector<int> data;
    Image() : id(0) {}
    Image(int i, int size) : id(i), data(size, 0) {}
};

void example2_image_processing() {
    cout << "\n=== Example 2: Image Processing Pipeline ===" << endl;
    
    const int NUM_IMAGES = 20;
    const int IMAGE_SIZE = 1000000;
    int generated = 0;
    
    auto start = high_resolution_clock::now();
    
    parallel_pipeline(
        8,  // tokens
        
        // Stage 1: Load images
        make_filter<void, Image*>(
            filter::serial_in_order,
            [&](flow_control& fc) -> Image* {
                if(generated < NUM_IMAGES) {
                    Image* img = new Image(generated++, IMAGE_SIZE);
                    return img;
                } else {
                    fc.stop();
                    return nullptr;
                }
            }
        )
        &
        // Stage 2: Apply filter (parallel)
        make_filter<Image*, Image*>(
            filter::parallel,
            [](Image* img) -> Image* {
                for(int& pixel : img->data)
                    pixel = sin(pixel * 0.01) * 255;
                return img;
            }
        )
        &
        // Stage 3: Compress (parallel)
        make_filter<Image*, Image*>(
            filter::parallel,
            [](Image* img) -> Image* {
                // Simulated compression
                for(int& pixel : img->data)
                    pixel = pixel / 16;
                return img;
            }
        )
        &
        // Stage 4: Save (serial)
        make_filter<Image*, void>(
            filter::serial_in_order,
            [](Image* img) {
                // cout << "Saved image " << img->id << endl;
                delete img;
            }
        )
    );
    
    auto elapsed = duration_cast<milliseconds>(
        high_resolution_clock::now() - start).count();
    
    cout << "Processed " << NUM_IMAGES << " images in " 
         << elapsed << " ms" << endl;
}

int main() {
    cout << "╔════════════════════════════════════════════════════════╗" << endl;
    cout << "║   TBB parallel_pipeline - Complete Tutorial           ║" << endl;
    cout << "╚════════════════════════════════════════════════════════╝" << endl;
    
    example1_basic();
    example2_image_processing();
    
    cout << "\n╔════════════════════════════════════════════════════════╗" << endl;
    cout << "║                    KEY TAKEAWAYS                       ║" << endl;
    cout << "╠════════════════════════════════════════════════════════╣" << endl;
    cout << "║                                                        ║" << endl;
    cout << "║  1. Pipeline processes items through multiple stages   ║" << endl;
    cout << "║  2. max_tokens controls how many items in flight       ║" << endl;
    cout << "║  3. filter::serial_in_order for ordered serial stages ║" << endl;
    cout << "║  4. filter::parallel for parallel stages               ║" << endl;
    cout << "║  5. Great for streaming data processing                ║" << endl;
    cout << "║                                                        ║" << endl;
    cout << "╚════════════════════════════════════════════════════════╝" << endl;
    
    return 0;
}
