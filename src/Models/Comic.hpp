#pragma once

#include <memory>
#include <string>
#include <vector>

class Comic {
public:
    Comic() = default;
    Comic(std::string name, std::string path) : name_(std::move(name)), path_(std::move(path)) {}

    auto getName() const -> const std::string & { return name_; }
    auto getPath() const -> const std::string & { return path_; }

    auto getImageFiles() const -> const std::vector<std::string> & { return imageFiles_; }
    void setImageFiles(std::vector<std::string> files) { imageFiles_ = std::move(files); }

    auto getCurrentPage() const -> int { return currentPage_; }
    void setCurrentPage(int page) { currentPage_ = page; }

    auto getTotalPages() const -> int { return static_cast<int>(imageFiles_.size()); }

private:
    std::string name_;
    std::string path_;
    std::vector<std::string> imageFiles_;
    int currentPage_ = 0;
};

using ComicPtr = std::shared_ptr<Comic>;
using ComicsVector = std::vector<ComicPtr>;