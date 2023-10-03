#pragma once

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "EPubDefs.hpp"

class EPubOpf {
public:
    EPubOpf(const char *path) : path_(path) {}

    void parse(const std::shared_ptr<uint8_t[]> &buffer, uint32_t length);

    struct ManifestItem {
        std::string href;
        std::string mediaType;
    };

    struct SpineItem {
        ManifestItem *item;
        // uint32_t uncompressedSize; // Must be loaded seprately
        // uint32_t offset;
    };

    // [[nodiscard]] auto getTotalSize() const -> uint32_t { return totalSize_; }

    // [[nodiscard]] auto getSpineIdxForOffset(uint32_t offset) const -> Idx;

    [[nodiscard]] auto getSpine(Idx idx) const -> const SpineItem & { return spine_[idx]; }

    [[nodiscard]] auto getFullPath(const std::string &fileName) const -> std::string {
        return basePath_ + fileName;
    }

    [[nodiscard]] inline auto getSpineCount() const -> uint16_t { return spine_.size(); }

    void extractPath(const char *fname, std::string &path);

private:
    const char *path_;

    std::string basePath_;

    using ManifestMap = std::unordered_map<std::string, ManifestItem>;
    using ManifestIterator = std::unordered_map<std::string, ManifestItem>::iterator;
    using SpineVector = std::vector<SpineItem>;

    std::string title_;
    std::string creator_;
    ManifestMap manifest_;
    SpineVector spine_;

    // uint32_t totalSize_{0};
};
