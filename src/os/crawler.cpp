#include "os/crawler.hpp"
#include <iostream>
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>

namespace FastSearch::OS {

Crawler::Crawler(fs::path root_path) : root(std::move(root_path)) {}

bool Crawler::is_accessible(const fs::path& p) noexcept {
    std::error_code ec;
    return fs::exists(p, ec) && !ec;
}

size_t Crawler::crawl(std::vector<std::string>& out_paths) {
    // Thread Pool Resources
    std::queue<fs::path> dir_queue;
    std::mutex queue_mutex;
    std::condition_variable cv;
    
    // Tracks how many threads are actively processing a folder. 
    // When this hits 0 and the queue is empty, the crawl is finished.
    std::atomic<int> active_tasks{0}; 
    std::mutex results_mutex;

    // Prime the pump with the root directory
    dir_queue.push(root);
    active_tasks++;

    // Dynamically grab the number of CPU cores your machine has
    unsigned int num_threads = std::thread::hardware_concurrency();
    if (num_threads == 0) num_threads = 8; // Fallback
    
    std::vector<std::thread> workers;

    for (unsigned int i = 0; i < num_threads; ++i) {
        workers.emplace_back([&]() {
            // We batch results locally to prevent 8 threads from constantly fighting 
            // over the main out_paths mutex for every single file.
            std::vector<std::string> local_paths; 
            auto options = fs::directory_options::skip_permission_denied;

            while (true) {
                fs::path current_dir;
                
                {
                    std::unique_lock<std::mutex> lock(queue_mutex);
                    // The thread goes to sleep here until there is work to do, 
                    // or until the whole job is completely finished.
                    cv.wait(lock, [&]() { return !dir_queue.empty() || active_tasks == 0; });

                    if (dir_queue.empty() && active_tasks == 0) {
                        break; // Kill the thread, work is done!
                    }

                    current_dir = dir_queue.front();
                    dir_queue.pop();
                }

                // Actually scan the directory (Non-recursively, just the top level)
                std::error_code ec;
                for (const auto& entry : fs::directory_iterator(current_dir, options, ec)) {
                    if (ec) continue; // Skip locked Windows files

                    if (entry.is_regular_file(ec)) {
                        local_paths.push_back(entry.path().string());
                    } else if (entry.is_directory(ec)) {
                        // Found a new folder! Push it to the queue for ANY thread to grab.
                        std::lock_guard<std::mutex> lock(queue_mutex);
                        dir_queue.push(entry.path());
                        active_tasks++;
                        cv.notify_one(); // Wake up one sleeping thread
                    }
                }

                // Flush this thread's local batch into the global results
                if (!local_paths.empty()) {
                    std::lock_guard<std::mutex> lock(results_mutex);
                    out_paths.insert(out_paths.end(), local_paths.begin(), local_paths.end());
                    local_paths.clear();
                }

                // Mark this specific task as complete
                active_tasks--;
                if (active_tasks == 0) {
                    cv.notify_all(); // Wake up everyone so they can safely exit
                }
            }
        });
    }

    // Wait for all 8-16 CPU threads to finish their execution
    for (auto& t : workers) {
        if (t.joinable()) {
            t.join();
        }
    }

    return out_paths.size();
}

} // namespace FastSearch::OS