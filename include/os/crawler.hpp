#ifndef CRAWLER_HPP
#define CRAWLER_HPP

#include <string>
#include <vector>
#include <filesystem>

namespace fs = std::filesystem;

namespace FastSearch::OS {

class Crawler {
public:
    explicit Crawler(fs::path root_path);
    ~Crawler() = default;

    Crawler(const Crawler&) = delete;
    Crawler& operator=(const Crawler&) = delete;

    size_t crawl(std::vector<std::string>& out_paths);

private:
    fs::path root;
    bool is_accessible(const fs::path& p) noexcept;
};

} // namespace FastSearch::OS

#endif // CRAWLER_HPP