#pragma once

#include <cstring>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <zlib.h>

#include "SpiramAllocator.hpp"
#include "sindarin-debug.h"

#define GZIP_CHUNK_SIZE 4096

class GZipStream {
public:
    GZipStream() : _chunkBuffer(NULL), _bytesInBuffer(0) {
        _chunkBuffer = (uint8_t *)SpiRamMalloc(GZIP_CHUNK_SIZE);
    }
    ~GZipStream() {
        if (_chunkBuffer) {
            SpiRamFree(_chunkBuffer);
            _chunkBuffer = NULL;
        }
    }

    auto init() -> bool;
    auto write(uint8_t *dataToCompress, int dataToCompressLength, uint8_t *outputBuffer,
               int *outputBufferLength, int outputBufferSize) -> bool;
    auto flush(uint8_t *outputBuffer, int *outputBufferLength, int outputBufferSize) -> bool;

protected:
    uint8_t *_chunkBuffer;
    int _bytesInBuffer;
    z_stream _zs;

    auto write(uint8_t *dataToCompress, int dataToCompressLength, uint8_t *outputBuffer,
               int *outputBufferLength, int outputBufferSize, bool flush) -> bool;

    auto compressBlock(int blockSize, uint8_t *outputBuffer, int *outputBufferLength,
                       int outputBufferSize, bool flush) -> bool;
};
