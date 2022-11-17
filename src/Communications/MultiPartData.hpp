#pragma once

#include <map>
#include <string>

#include "HTTPClient.h"
#include "Storage/FileSystem.hpp"

class MultiPartData {
public:
    MultiPartData(FileSystem &fileSystem) : fileSystem_(fileSystem) {}
    ~MultiPartData() {
        // close and delete the temp file
        tempFile_.close();
        fileSystem_.remove(fileName_);
    }

    void addKeyValue(const std::string &key, const std::string &value);
    void addKeyValue(const std::string &key, uint8_t *value, int len, bool isFile);
    void startFile(const std::string &key);
    void writeFileChunk(uint8_t *chunk, int len);
    void endFile();
    void endMultiPartData();

    auto getContentType() -> String;
    auto getFile() -> File;

private:
    FileSystem &fileSystem_;
    std::string boundary_ = "SIND12345";
    std::string fileName_ = "/temp.coredump";
    bool keyWritten_{false};
    File tempFile_;
    char tmp_[1024];

    void startKeyWrite();
    void writeHeader(const std::string &key, bool isFile);
    void writeFooter();
};