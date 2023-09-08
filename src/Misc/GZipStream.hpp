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
    GZipStream() {}
    ~GZipStream() = default;

    auto init() -> bool;

    auto write(uint8_t *dataToCompress, int dataToCompressLength) -> bool;
    auto flush() -> bool;

    auto available() -> int { return outputAvailable_; }
    auto read(uint8_t *destBuffer, int destBufferSize) -> int;

    auto initialized() -> bool { return init_; }

protected:
    auto write(uint8_t *dataToCompress, int dataToCompressLength, bool flush) -> bool;
    auto compressBlock(uint8_t *inputBuffer, int blockSize, uint8_t *outputBuffer,
                       int *outputBufferLength, int outputBufferSize, bool flush, bool &done)
        -> bool;

    int outputAvailable_{0};
    std::shared_ptr<uint8_t[]> outputBuffer_;
    z_stream zs_;
    bool init_{false};
};
