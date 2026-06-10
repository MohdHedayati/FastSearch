#ifndef INVERTED_INDEX_HPP
#define INVERTED_INDEX_HPP

#include <string>
#include <vector>
#include <unordered_map>
#include <set>
#include <shared_mutex>

namespace FastSearch::Core {

class InvertedIndex {
public:
    InvertedIndex() = default;
    ~InvertedIndex() = default;

    // Read a file, parse its words, and add them to the index
    void index_file(const std::string& filepath);
    
    // Look up a word and get all files containing it
    std::vector<std::string> search_word(const std::string& word) const;

private:
    // Maps a word -> Set of file paths
    std::unordered_map<std::string, std::set<std::string>> index_map;
    mutable std::shared_mutex rw_lock;

    // Helper to prevent us from trying to parse a 2GB .mp4 video as text
    bool is_indexable_text_file(const std::string& filepath) const;
};

} // namespace FastSearch::Core

#endif // INVERTED_INDEX_HPP