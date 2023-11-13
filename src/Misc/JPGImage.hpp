#if USE_JPG_DECODER
#pragma once

#include "Misc/SpiramAllocator.hpp"

class JPGImage {
public:
    JPGImage() {
        outputBuffer_ = std::shared_ptr<uint8_t[]>(
            (uint8_t *)SpiRamMalloc(sizeof(uint8_t) * 256 * 256 * 3), free);
    }
    ~JPGImage() = default;

    auto readFromBytes(uint8_t *jpgbytes, int size) -> bool;
    auto getBitsPerPixel() -> int;
    auto getBuffer() -> uint8_t *;

private:
    std::shared_ptr<uint8_t[]> outputBuffer_;
};
#endif