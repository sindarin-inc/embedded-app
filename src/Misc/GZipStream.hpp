#pragma once

#include <cstring>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <zlib.h>

#include "SpiramAllocator.hpp"

class GZipStream {
public:
    GZipStream() = default;
    ~GZipStream() = default;

    auto init() -> bool;

    auto write(uint8_t *dataToCompress, int dataToCompressLength, uint8_t *outputBuffer,
               int *outputBufferLength, int outputBufferSize, bool &done) -> bool;
    auto flush(uint8_t *outputBuffer, int *outputBufferLength, int outputBufferSize, bool &done)
        -> bool;

protected:
    auto compressBlock(uint8_t *inputBuffer, int blockSize, uint8_t *outputBuffer,
                       int *outputBufferLength, int outputBufferSize, bool flush, bool &done)
        -> bool;

    z_stream zs_;
};
