#pragma once

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <cstring>
#include <zlib.h>

#define GZIP_CHUNK_SIZE 4096

class GZipStream {
public:
    GZipStream() : _chunkBuffer(NULL), _bytesInBuffer(0) { _chunkBuffer = (uint8_t *)malloc(GZIP_CHUNK_SIZE); }
        //SpiRamMalloc(GZIP_CHUNK_SIZE); };
    ~GZipStream() {
        if (_chunkBuffer) {
            free(_chunkBuffer);
            _chunkBuffer = NULL;
            //SpiRamFree(_chunkBuffer);
        }
    }

    bool init();

    auto write(uint8_t *dataToCompress, int dataToCompressLength, uint8_t *outputBuffer,
               int *outputBufferLength, int outputBufferSize) -> bool;
    auto flush(uint8_t *outputBuffer,
               int *outputBufferLength, int outputBufferSize) -> bool;

protected:
    uint8_t *_chunkBuffer;
    int _bytesInBuffer;

    auto write(uint8_t *dataToCompress, int dataToCompressLength, uint8_t *outputBuffer,
               int *outputBufferLength, int outputBufferSize, bool flush) -> bool;

    auto compressBlock(int blockSize, uint8_t* outputBuffer, int *outputBufferLength, int outputBufferSize, bool flush) -> bool;

    z_stream zs;

};
