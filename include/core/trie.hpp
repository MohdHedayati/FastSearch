#ifndef TRIE_HPP
#define TRIE_HPP

#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include <shared_mutex>
#include <fstream> 

namespace FastSearch::Core {

struct TrieNode {
    std::unordered_map<char, std::unique_ptr<TrieNode>> children;
    bool is_end_of_path = false;
    std::string full_path = ""; 
};

class Trie {
public:
    Trie();
    ~Trie() = default;

    Trie(const Trie&) = delete;
    Trie& operator=(const Trie&) = delete;

    void insert(const std::string& full_path);
    std::vector<std::string> search_prefix(const std::string& prefix) const;

    // --- Fuzzy Search API ---
    std::vector<std::string> search_fuzzy(const std::string& query, int max_edits = 2) const;

    // --- Binary Serialization API ---
    bool save_to_disk(const std::string& filepath) const;
    bool load_from_disk(const std::string& filepath);

private:
    std::unique_ptr<TrieNode> root;
    mutable std::shared_mutex rw_lock; 

    void collect_all_paths(const TrieNode* node, std::vector<std::string>& results) const;

    // --- Fuzzy DP Helper ---
    void search_fuzzy_recursive(const TrieNode* node, char letter, const std::string& query, 
                                const std::vector<int>& previous_row, 
                                std::vector<std::string>& results, int max_edits) const;

    // --- Recursive Binary Helpers ---
    void serialize_node(std::ofstream& out, const TrieNode* node) const;
    void deserialize_node(std::ifstream& in, TrieNode* node);
};

} // namespace FastSearch::Core

#endif // TRIE_HPP