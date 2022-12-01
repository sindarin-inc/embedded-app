#pragma once

#include <map>
#include <stdint.h>
#include <string>
#include <unzipLIB.h>

#include "Storage/FileSystem.hpp"

class ZipFile {
public:
    ZipFile(FileSystem &fs, const char *filename) : fileSystem_(fs), fileName_(filename) {}
    ~ZipFile() {}

    bool open();
    void close();
    void listFiles();

    // static callbacks for unzipLIB library
    static void *zipOpen(const char *filename, int32_t *size);
    static void zipClose(void *p);
    static int32_t zipRead(void *p, uint8_t *buffer, int32_t length);
    static int32_t zipSeek(void *p, int32_t position, int iType);

protected:
    FileSystem &fileSystem_;
    std::string fileName_;
    UNZIP zip_;
    File fp_;

    static std::map<std::string, ZipFile *> zipRegistry;
};