#pragma once

// #include <expat.h>
#include <map>
#include <string>
#include <vector>

#include "Misc/ZipFile.hpp"

class EPubOpf {
public:
    EPubOpf(ZipFile &zipFile, const char *path) : path_(path), zipFile_(zipFile) {}
    void parse();

    struct ManifestItem {
        std::string href;
        std::string mediaType;
    };

    struct SpineItem {
        ManifestItem *item;
        uint32_t uncompressedSize; // Must be loaded seprately
        uint32_t offset;
    };

    [[nodiscard]] auto getTotalSize() const -> uint32_t { return totalSize_; }

    [[nodiscard]] auto getSpineIdxForOffset(uint32_t offset) const -> uint16_t;

    [[nodiscard]] auto getSpine(uint16_t idx) const -> const SpineItem & { return spine_[idx]; }

    [[nodiscard]] auto getFullPath(std::string fileName) const -> std::string {
        return basePath_ + fileName;
    }

private:
    const char *path_;
    ZipFile &zipFile_;

    std::string basePath_;

    using ManifestMap = std::map<std::string, ManifestItem>;
    using ManifestIterator = std::map<std::string, ManifestItem>::iterator;
    using SpineVector = std::vector<SpineItem>;

    std::string title_;
    std::string creator_;
    ManifestMap manifest_;
    SpineVector spine_;

    uint32_t totalSize_{0};

    void extract_path(const char *fname, std::string &path);
};
