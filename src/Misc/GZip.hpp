#pragma once

#include <string>
#include <stdio.h>
#include <cstdint>
#include <zlib.h>

class GZip {
public:
    GZip() : gf_(nullptr) {}
    ~GZip() = default;

    auto initWithOutputFileName(const std::string &outputFileName) -> bool;
    auto initWithOutputFile(FILE* fp) -> bool;
    auto initWithMemory(uint8_t* buf, int size) -> bool;

    auto writeMemory(uint8_t* mem, int size) -> bool;
    auto compressFile(const std::string &inputFileName) -> bool;
    auto close() -> bool;

protected:
    gzFile gf_;
};
