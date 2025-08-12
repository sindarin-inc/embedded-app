#pragma once

#ifdef SIMULATOR

#include <memory>
#include <string>
#include "Storage/FileSystem.hpp"
#include "Displays/DisplaySystem.hpp"

class JPGImageSTB {
public:
    JPGImageSTB();
    ~JPGImageSTB() = default;

    auto load(const std::string &filePath, FileSystem &fs) -> bool;
    void draw(DisplaySystem &display, int x, int y);
    [[nodiscard]] auto getWidth() const -> int { return width_; }
    [[nodiscard]] auto getHeight() const -> int { return height_; }
    [[nodiscard]] auto getBitsPerPixel() const -> int { return bitsPerPixel_; }
    auto getBuffer() -> uint8_t * { return outputBuffer_.get(); }

private:
    std::shared_ptr<uint8_t[]> outputBuffer_;
    int width_{0};
    int height_{0};
    int bitsPerPixel_{0};
    
    auto loadFromBytes(const uint8_t *jpgData, int size) -> bool;
};

#endif