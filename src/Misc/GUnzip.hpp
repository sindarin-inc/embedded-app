#pragma once

#include <cstdint>
//#include <zlib.h>
#include "miniz.h"

#include "Storage/FileSystem.hpp"

class GUnzip {
public:
    GUnzip() : init_(false) {}
    ~GUnzip() = default;

    auto decompressFile(FileSystem &fileSystem, const std::string &inputFileName,
                        const std::string &outputFileName) -> bool;

    auto decompressMemory(uint8_t *input, int inputSize) -> bool;

    auto decompress(uint8_t *input, int inputSize, uint8_t *output, int outputSize, int &inputRead,
                    int &outputAvail) -> int;

    auto init() -> bool;

protected:
    mz_stream zstr_;
    bool init_;
    //std::unique_ptr<uint8_t[]> inflateBuf_;
};
