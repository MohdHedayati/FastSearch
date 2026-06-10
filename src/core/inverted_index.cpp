#include "core/inverted_index.hpp"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cctype>
#include <mutex>
#include <filesystem> // <-- Added for file size checking

namespace fs = std::filesystem;

namespace FastSearch::Core {

bool InvertedIndex::is_indexable_text_file(const std::string& filepath) const {
    const std::vector<std::string> text_extensions = {
        ".txt", ".md", ".cpp", ".hpp", ".h", ".c", ".py", ".json", ".xml", ".csv"
    };

    size_t pos = filepath.find_last_of(".");
    if (pos == std::string::npos) return false;

    std::string ext = filepath.substr(pos);
    for (char& c : ext) c = static_cast<char>(std::tolower(c));

    if (std::find(text_extensions.begin(), text_extensions.end(), ext) == text_extensions.end()) {
        return false; // Not a text extension
    }

    // --- NEW: THE MONSTER FILE TRAP ---
    // Do not attempt to parse files larger than 5 MB (5 * 1024 * 1024 bytes)
    std::error_code ec;
    uintmax_t file_size = fs::file_size(filepath, ec);
    if (ec || file_size > 5242880) { 
        return false;
    }

    return true;
}

void InvertedIndex::index_file(const std::string& filepath) {
    if (!is_indexable_text_file(filepath)) return;

    std::ifstream file(filepath);
    if (!file.is_open()) return;

    std::string word;
    std::set<std::string> unique_words_in_file;

    while (file >> word) {
        std::string clean_word = "";
        for (char c : word) {
            if (std::isalnum(c)) {
                clean_word += static_cast<char>(std::tolower(c));
            }
        }
        
        if (!clean_word.empty()) {
            unique_words_in_file.insert(clean_word);
        }
    }

    if (!unique_words_in_file.empty()) {
        std::unique_lock<std::shared_mutex> lock(rw_lock);
        for (const auto& w : unique_words_in_file) {
            index_map[w].insert(filepath);
        }
    }
}

std::vector<std::string> InvertedIndex::search_word(const std::string& word) const {
    std::shared_lock<std::shared_mutex> lock(rw_lock);
    
    std::string clean_word = "";
    for (char c : word) {
        if (std::isalnum(c)) clean_word += static_cast<char>(std::tolower(c));
    }

    auto it = index_map.find(clean_word);
    if (it != index_map.end()) {
        return std::vector<std::string>(it->second.begin(), it->second.end());
    }

    return {}; 
}

} // namespace FastSearch::Core