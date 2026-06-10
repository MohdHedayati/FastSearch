#include <iostream>
#include <chrono>
#include <vector>
#include <string>
#include <cstdlib>
#include <filesystem>
#include "os/crawler.hpp"
#include "os/watcher.hpp"
#include "os/ipc_server.hpp"
#include "core/trie.hpp"
#include "core/inverted_index.hpp"

namespace fs = std::filesystem;

int main() {
    const char* user_profile = std::getenv("USERPROFILE");
    std::string target_dir = user_profile ? std::string(user_profile) : "C:\\";
    std::string index_file = "fast_search_index.bin";

    std::cout << "[*] FastSearch Daemon Booting...\n";
    
    FastSearch::Core::Trie index;
    FastSearch::Core::InvertedIndex deep_index;

    // --- 1. Load the Filename Trie ---
    if (fs::exists(index_file)) {
        std::cout << "[*] Loading binary filename index...\n";
        index.load_from_disk(index_file);
    } else {
        std::cout << "[*] No index found. Crawling disk...\n";
        FastSearch::OS::Crawler crawler(target_dir);
        std::vector<std::string> files;
        crawler.crawl(files);
        for (const auto& file : files) index.insert(file);
        index.save_to_disk(index_file);
    }

    // --- 2. Build the Content Index ---
    std::cout << "[*] Building Deep Content Index (Parsing text files < 5MB)...\n";
    auto start_deep = std::chrono::high_resolution_clock::now();
    
    std::vector<std::string> all_files = index.search_prefix(""); 
    
    // --- NEW: TELEMETRY STREAM ---
    int total_files = all_files.size();
    int processed_count = 0;

    for (const auto& file : all_files) {
        deep_index.index_file(file); 
        
        processed_count++;
        // Print an update every 10,000 files so we know it hasn't frozen
        if (processed_count % 10000 == 0) {
            std::cout << "    -> Checked " << processed_count << " / " << total_files << " files...\n";
        }
    }
    
    auto end_deep = std::chrono::high_resolution_clock::now();
    std::cout << "[+] Deep Index built in " 
              << std::chrono::duration_cast<std::chrono::milliseconds>(end_deep - start_deep).count() << " ms.\n";

    // --- 3. Start the Background Services ---
    std::cout << "[*] Attaching Win32 Kernel Watcher...\n";
    FastSearch::OS::Watcher live_watcher(target_dir, index);
    live_watcher.start();

    std::cout << "[*] Opening IPC Named Pipe...\n";
    FastSearch::OS::IPCServer ipc_server(index, deep_index); 
    ipc_server.start();

    std::cout << "\n[+] DAEMON ACTIVE AND LISTENING IN BACKGROUND.\n";
    std::cout << "    Leave this window open. Use the 'fs' client in a new terminal to search.\n";
    std::cout << "    Press ENTER here to safely shutdown the server...\n";
    
    std::cin.get();

    std::cout << "[*] Shutting down services...\n";
    return 0;
}