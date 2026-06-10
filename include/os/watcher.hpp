#ifndef WATCHER_HPP
#define WATCHER_HPP

#include <string>
#include <thread>
#include <atomic>
#include "core/trie.hpp"

namespace FastSearch::OS {

class Watcher {
public:
    Watcher(std::string target_dir, Core::Trie& index);
    ~Watcher();

    Watcher(const Watcher&) = delete;
    Watcher& operator=(const Watcher&) = delete;

    void start();
    void stop();

private:
    std::string directory;
    Core::Trie& shared_index;
    std::atomic<bool> running;
    std::thread background_thread;

    // The low-level Windows API loop
    void watch_loop(); 
};

} // namespace FastSearch::OS

#endif // WATCHER_HPP