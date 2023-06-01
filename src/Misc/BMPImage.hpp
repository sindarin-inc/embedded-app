#pragma once

#include <Misc/SpiramAllocator.hpp>

class BMPImage {
public:
    BMPImage() : bitsPerPixel_(0), width_(0), height_(0) {
        outputBuffer_ = std::shared_ptr<uint8_t[]>(
            (uint8_t *)SpiRamMalloc(sizeof(uint8_t) * 256 * 256), free);
    }
    ~BMPImage() = default;

    auto readFromBytes(uint8_t *bmpbytes, int size) -> bool;
    auto getWidth() -> int;
    auto getHeight() -> int;
    auto getBitsPerPixel() -> int;
    auto getBuffer() -> uint8_t *;

private:
    std::shared_ptr<uint8_t[]> outputBuffer_;
    int bitsPerPixel_;
    int width_;
    int height_;
};