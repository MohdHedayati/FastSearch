#include "core/trie.hpp"
#include <cctype>
#include <mutex> // For unique_lock and shared_lock
#include <algorithm> // For std::min used in Levenshtein DP

namespace FastSearch::Core {

Trie::Trie() : root(std::make_unique<TrieNode>()) {}

void Trie::insert(const std::string& full_path) {
    // WRITE LOCK: Only ONE thread can be in this block at a time.
    std::unique_lock<std::shared_mutex> lock(rw_lock);

    TrieNode* current = root.get();
    size_t pos = full_path.find_last_of("/\\");
    std::string filename = (pos == std::string::npos) ? full_path : full_path.substr(pos + 1);

    for (char c : filename) {
        c = static_cast<char>(std::tolower(c));
        if (current->children.find(c) == current->children.end()) {
            current->children[c] = std::make_unique<TrieNode>();
        }
        current = current->children[c].get();
    }
    
    current->is_end_of_path = true;
    current->full_path = full_path;
}

std::vector<std::string> Trie::search_prefix(const std::string& prefix) const {
    // READ LOCK: Multiple threads can search at the exact same time, 
    // but they must wait if a Write Lock is currently active.
    std::shared_lock<std::shared_mutex> lock(rw_lock);

    std::vector<std::string> results;
    TrieNode* current = root.get();

    for (char c : prefix) {
        c = static_cast<char>(std::tolower(c));
        if (current->children.find(c) == current->children.end()) {
            return results; 
        }
        current = current->children[c].get();
    }

    collect_all_paths(current, results);
    return results;
}

void Trie::collect_all_paths(const TrieNode* node, std::vector<std::string>& results) const {
    if (!node) return;
    if (node->is_end_of_path) {
        results.push_back(node->full_path);
    }
    for (const auto& [ch, childNode] : node->children) {
        collect_all_paths(childNode.get(), results);
    }
}

// --- Binary Serialization Implementation ---

bool Trie::save_to_disk(const std::string& filepath) const {
    // Read lock because we are just looking at the tree, not modifying it
    std::shared_lock<std::shared_mutex> lock(rw_lock);
    
    // Open file in binary writing mode
    std::ofstream out(filepath, std::ios::binary);
    if (!out.is_open()) return false;

    serialize_node(out, root.get());
    return true;
}

void Trie::serialize_node(std::ofstream& out, const TrieNode* node) const {
    // 1. Write the boolean flag
    out.write(reinterpret_cast<const char*>(&node->is_end_of_path), sizeof(bool));
    
    // 2. If it's the end of a path, write the string length, then the string bytes
    if (node->is_end_of_path) {
        size_t len = node->full_path.size();
        out.write(reinterpret_cast<const char*>(&len), sizeof(size_t));
        out.write(node->full_path.data(), len);
    }

    // 3. Write how many children this node has
    size_t num_children = node->children.size();
    out.write(reinterpret_cast<const char*>(&num_children), sizeof(size_t));

    // 4. Recursively write each child (the character key, then the node itself)
    for (const auto& [ch, childNode] : node->children) {
        out.write(&ch, sizeof(char));
        serialize_node(out, childNode.get());
    }
}

bool Trie::load_from_disk(const std::string& filepath) {
    // Write lock because we are actively building the tree in memory
    std::unique_lock<std::shared_mutex> lock(rw_lock);

    // Open file in binary reading mode
    std::ifstream in(filepath, std::ios::binary);
    if (!in.is_open()) return false;

    // Wipe the existing tree clean before loading
    root = std::make_unique<TrieNode>();
    deserialize_node(in, root.get());
    return true;
}

void Trie::deserialize_node(std::ifstream& in, TrieNode* node) {
    // 1. Read the boolean flag
    in.read(reinterpret_cast<char*>(&node->is_end_of_path), sizeof(bool));

    // 2. If it's a file path, read the length, then allocate the string
    if (node->is_end_of_path) {
        size_t len;
        in.read(reinterpret_cast<char*>(&len), sizeof(size_t));
        node->full_path.resize(len);
        in.read(&node->full_path[0], len);
    }

    // 3. Read how many children this node has
    size_t num_children;
    in.read(reinterpret_cast<char*>(&num_children), sizeof(size_t));

    // 4. Recursively reconstruct the children
    for (size_t i = 0; i < num_children; ++i) {
        char ch;
        in.read(&ch, sizeof(char));
        node->children[ch] = std::make_unique<TrieNode>();
        deserialize_node(in, node->children[ch].get());
    }
}

// --- Fuzzy Matching Implementation ---

std::vector<std::string> Trie::search_fuzzy(const std::string& query, int max_edits) const {
    std::shared_lock<std::shared_mutex> lock(rw_lock);
    std::vector<std::string> results;

    // 1. Dynamic Edits: Scale the allowed typos based on query length to prevent memory flooding
    int dynamic_edits = max_edits;
    if (query.length() <= 3) dynamic_edits = 0;       // Exact match only for tiny queries
    else if (query.length() <= 6) dynamic_edits = 1;  // 1 typo allowed for medium queries

    std::vector<int> current_row(query.length() + 1);
    for (size_t i = 0; i <= query.length(); ++i) {
        current_row[i] = i;
    }

    std::string lower_query = query;
    for (char& c : lower_query) c = static_cast<char>(std::tolower(c));

    for (const auto& [ch, childNode] : root->children) {
        search_fuzzy_recursive(childNode.get(), ch, lower_query, current_row, results, dynamic_edits);
    }

    return results;
}

void Trie::search_fuzzy_recursive(const TrieNode* node, char letter, const std::string& query, 
                                  const std::vector<int>& previous_row, 
                                  std::vector<std::string>& results, int max_edits) const {
    
    size_t columns = query.length() + 1;
    std::vector<int> current_row(columns);
    current_row[0] = previous_row[0] + 1;

    int min_distance = current_row[0];

    for (size_t c = 1; c < columns; ++c) {
        int insert_cost = current_row[c - 1] + 1;
        int delete_cost = previous_row[c] + 1;
        int replace_cost = previous_row[c - 1] + (query[c - 1] != letter ? 1 : 0);

        current_row[c] = std::min({insert_cost, delete_cost, replace_cost});
        min_distance = std::min(min_distance, current_row[c]);
    }

    // 2. The Prefix Trigger: If the prefix spelled SO FAR matches the query within our edit limit...
    if (current_row.back() <= max_edits) {
        // We found a valid fuzzy prefix! Collect everything underneath it immediately.
        collect_all_paths(node, results);
        return; // Stop calculating DP for this branch, we already got the files!
    }

    // 3. Pruning: Only continue down the branch if it's mathematically possible to find a match
    if (min_distance <= max_edits) {
        for (const auto& [ch, childNode] : node->children) {
            search_fuzzy_recursive(childNode.get(), ch, query, current_row, results, max_edits);
        }
    }
}

} // namespace FastSearch::Core