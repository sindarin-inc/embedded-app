#pragma once

#include <cstdint>
#include <stdio.h>
#include <string>
#include <zlib.h>

class GZip {
public:
    GZip() {}
    ~GZip() = default;

    auto initWithOutputFileName(const std::string &outputFileName) -> bool;
    auto initWithOutputFile(FILE *fp) -> bool;

    auto writeMemory(uint8_t *mem, int size) -> bool;
    auto compressFile(const std::string &inputFileName) -> bool;
    auto close() -> bool;

protected:
    gzFile gf_{nullptr};
};
