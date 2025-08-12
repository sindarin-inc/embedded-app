#if USE_JPG_DECODER
#pragma once

#include "Misc/SpiramAllocator.hpp"
#include "Storage/FileSystem.hpp"
#include <string>

class DisplaySystem;

class JPGImage {
public:
    JPGImage() = default;
    ~JPGImage() = default;

    auto readFromBytes(uint8_t *jpgbytes, int size) -> bool;
    auto load(const std::string &filePath, FileSystem &fs) -> bool;
    auto getBitsPerPixel() -> int;
    auto getBuffer() -> uint8_t *;
    auto getWidth() const -> int { return width_; }
    auto getHeight() const -> int { return height_; }
    void draw(DisplaySystem &display, int x, int y);

private:
    std::shared_ptr<uint8_t[]> outputBuffer_;
    int width_ = 0;
    int height_ = 0;
    int bitsPerPixel_ = 24;
};
#endif