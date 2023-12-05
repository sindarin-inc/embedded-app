#pragma once

#include <string>
#include <unordered_map>
#include <utility>

#include "Storage/FileSystem.hpp"

class MultiPartData {
public:
    MultiPartData(FileSystem &fileSystem, std::string fileName)
        : fileSystem_(fileSystem), fileName_(std::move(fileName)) {}
    ~MultiPartData() { tempFile_.close(); }

    auto addKeyValue(const std::string &key, const std::string &value) -> bool;
    auto addKeyValue(const std::string &key, const uint8_t *value, int len, bool isFile) -> bool;
    auto startFile(const std::string &key) -> bool;
    auto writeFileChunk(uint8_t *chunk, int len) -> bool;
    auto endFile() -> bool;
    auto endMultiPartData() -> bool;

    static auto getContentType() -> std::string;
    auto getFile() -> SFile;

private:
    FileSystem &fileSystem_;
    static std::string boundary;
    std::string fileName_;
    bool keyWritten_{false};
    SFile tempFile_;
    char tmp_[1024]{};

    auto startKeyWrite() -> bool;
    auto writeHeader(const std::string &key, bool isFile) -> bool;
    auto writeFooter() -> bool;
};
