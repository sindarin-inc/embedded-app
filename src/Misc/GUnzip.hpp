#pragma once

#include <cstdint>

#include "Storage/FileSystem.hpp"
#include "miniz.h"

class GUnzip {
public:
    GUnzip() {}
    ~GUnzip() = default;

    auto decompressFile(FileSystem &fileSystem, const std::string &inputFileName,
                        const std::string &outputFileName) -> bool;

    auto decompressMemory(uint8_t *input, int inputSize) -> bool;

    auto decompress(uint8_t *input, int inputSize, uint8_t *output, int outputSize, int &inputRead,
                    int &outputAvail) -> int;

    auto init() -> bool;

    void setCompressedSize(int s) { compressedSize_ = s; }

protected:
    mz_stream zstr_;
    bool init_{false};
    int compressedSize_{0};
    int compressedPos_{0};
    int uncompressedPos_{0};
    uint8_t buf_[1024];
    int bufPos_{0};

    bool readHeader_{false};
    int gzHeaderBytes_{0};
};
