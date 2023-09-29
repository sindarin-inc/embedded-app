#pragma once

#include <cstdint>
#include <miniz/miniz.h>

#include "Storage/FileSystem.hpp"

class GUnzip {
public:
    GUnzip() = default;
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
    int bufPos_{0};

    bool readHeader_{false};
    int gzHeaderBytes_{0};
};
