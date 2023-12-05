#pragma once

#include <cstring>
#include <miniz/miniz.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "SpiramAllocator.hpp"
#include "Storage/SFile.hpp"

class GZipStream {
public:
    GZipStream() = default;
    ~GZipStream() = default;

    auto init() -> bool;

    auto write(uint8_t *dataToCompress, int dataToCompressLength) -> bool;
    auto flush() -> bool;

    [[nodiscard]] auto available() const -> int { return outputAvailable_; }
    auto read(uint8_t *destBuffer, int destBufferSize) -> int;

    // interface for direct access to the compressed data, so a copy is not required.
    auto getDataPtr() -> uint8_t *;
    void advance(int size);

    [[nodiscard]] auto initialized() const -> bool { return init_; }

protected:
    auto write(uint8_t *dataToCompress, int dataToCompressLength, bool flush) -> bool;
    auto compressBlock(uint8_t *inputBuffer, int blockSize, uint8_t *outputBuffer,
                       int *outputBufferLength, int outputBufferSize, bool flush, bool &done)
        -> bool;

    int outputAvailable_{0};
    std::shared_ptr<uint8_t[]> outputBuffer_;
    mz_stream zs_{};
    mz_ulong crc_{MZ_CRC32_INIT};
    bool init_{false};
    uint32_t uncompressedSize_{0};
};
