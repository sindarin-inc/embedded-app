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

    [[nodiscard]] uint32_t getTotalSize() const { return totalSize_; }

private:
    const char *path_;
    ZipFile &zipFile_;

    struct ManifestItem {
        std::string href;
        std::string mediaType;
        [[nodiscard]] auto fullPath() const -> std::string { return "OEBPS/" + href; }
    };

    struct SpineItem {
        std::string idRef;
        uint32_t uncompressedSize; // Must be loaded seprately
    };

    using ManifestMap = std::map<std::string, ManifestItem>;
    using ManifestIterator = std::map<std::string, ManifestItem>::iterator;
    using SpineVector = std::vector<SpineItem>;

    std::string title_;
    std::string creator_;
    ManifestMap manifest_;
    SpineVector spine_;

    uint32_t totalSize_{0};
};
