#pragma once

#include <map>
#include <string>

#include "HTTPClient.h"
#include "Storage/FileSystem.hpp"

class MultiPartData {
public:
    MultiPartData(FileSystem &fileSystem, const std::string &fileName)
        : fileSystem_(fileSystem), fileName_(fileName) {}
    ~MultiPartData() { tempFile_.close(); }

    bool addKeyValue(const std::string &key, const std::string &value);
    bool addKeyValue(const std::string &key, uint8_t *value, int len, bool isFile);
    bool startFile(const std::string &key);
    bool writeFileChunk(uint8_t *chunk, int len);
    bool endFile();
    bool endMultiPartData();

    static auto getContentType() -> String;
    auto getFile() -> File;

private:
    FileSystem &fileSystem_;
    std::string boundary_ = "SIND12345";
    std::string fileName_;
    bool keyWritten_{false};
    File tempFile_;
    char tmp_[1024];

    bool startKeyWrite();
    bool writeHeader(const std::string &key, bool isFile);
    bool writeFooter();
};