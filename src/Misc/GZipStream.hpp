#pragma once

#include <cstring>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <zlib.h>

#include "SpiramAllocator.hpp"
#include "Storage/SFile.hpp"

class GZipStream {
public:
    GZipStream() : outputAvailable_(0), init_(false) {}
    ~GZipStream() = default;

    auto init() -> bool;

    auto write(uint8_t *dataToCompress, int dataToCompressLength, uint8_t *outputBuffer,
               int *outputBufferLength, int outputBufferSize, bool &done) -> bool;

    auto write(uint8_t *dataToCompress, int dataToCompressLength) -> bool;

    auto flush(uint8_t *outputBuffer, int *outputBufferLength, int outputBufferSize, bool &done)
        -> bool;

    auto flush() -> bool;

    int available() { return outputAvailable_; }
    // uint8_t *getOutputBuffer() { return outputBuffer_.get(); }

    int read(uint8_t *destBuffer, int destBufferSize);

    auto compressFileToMemory(SFile &file, uint8_t *outputBuffer, int outputBufferSize,
                              int &compressedSize) -> bool;

    bool initialized() { return init_; }

protected:
    auto write(uint8_t *dataToCompress, int dataToCompressLength, bool flush) -> bool;
    auto compressBlock(uint8_t *inputBuffer, int blockSize, uint8_t *outputBuffer,
                       int *outputBufferLength, int outputBufferSize, bool flush, bool &done)
        -> bool;

    int outputAvailable_;
    std::shared_ptr<uint8_t[]> outputBuffer_;
    z_stream zs_;
    bool init_;
};
