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
    GZipStream() : _chunkBuffer(NULL), _bytesInBuffer(0) {
        _chunkBuffer = (uint8_t *)SpiRamMalloc(GZIP_CHUNK_SIZE);
    }

    ~GZipStream() {
        if (_chunkBuffer) {
            SpiRamFree(_chunkBuffer);
            _chunkBuffer = NULL;
        }
    }

    bool init();

    auto write(uint8_t *dataToCompress, int dataToCompressLength, uint8_t *outputBuffer,
               int *outputBufferLength, int outputBufferSize, bool &done) -> bool;
    auto flush(uint8_t *outputBuffer, int *outputBufferLength, int outputBufferSize, bool &done)
        -> bool;

protected:
    uint8_t *_chunkBuffer;
    int _bytesInBuffer;

    auto compressBlock(uint8_t *inputBuffer, int blockSize, uint8_t *outputBuffer,
                       int *outputBufferLength, int outputBufferSize, bool flush, bool &done)
        -> bool;

    z_stream zs;
};
