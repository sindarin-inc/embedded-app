#pragma once

#include <cstdint>
#include <map>
#include <string>
#include <unzipLIB.h>

#include "Storage/FileSystem.hpp"

class ZipFile {
public:
    ZipFile(FileSystem &fs, const char *filename) : fileSystem_(fs), fileName_(filename) {}
    ~ZipFile() = default;

    auto open() -> bool;
    void close();
    void listFiles();
    void extractAllToDirectory(const char *dirname);

    // APIs for interacting with individual files
    auto openFile(const char *pathInZip) -> int32_t;
    auto readFromFile(uint8_t *buf, size_t size) -> int;
    auto getFileInfo(unz_file_info *fi, char *szName, size_t nameBufSize, char *szComment,
                     size_t commentBufSize) -> int;
    auto closeFile() -> int32_t;

    // static callbacks for unzipLIB library
    static auto zipOpen(const char *filename, int32_t *size) -> void *;
    static void zipClose(void *p);
    static auto zipRead(void *p, uint8_t *buffer, int32_t length) -> int32_t;
    static auto zipSeek(void *p, int32_t position, int iType) -> int32_t;

protected:
    FileSystem &fileSystem_;
    std::string fileName_;
    UNZIP zip_;
    File fp_;

    static std::map<std::string, ZipFile *> zipRegistry;

    void extractCurrentFile(const char *outFname);
};