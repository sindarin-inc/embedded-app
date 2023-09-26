#pragma once

#include <cstdint>

#include "Storage/FileSystem.hpp"
#include "miniz.h"

class GUnzip {
public:
    GUnzip()
        : init_(false), compressedSize_(0), compressedPos_(0), uncompressedPos_(0), bufPos_(0),
          readHeader_(false), gzHeaderBytes_(0) {}
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
    bool init_;
    int compressedSize_;
    int compressedPos_;
    int uncompressedPos_;
    uint8_t buf_[1024];
    int bufPos_;

    bool readHeader_;
    int gzHeaderBytes_;
};
