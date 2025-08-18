#if CONFIG_SOC_JPEG_DECODE_SUPPORTED
#pragma once

#include "Misc/SpiramAllocator.hpp"
#include "Storage/FileSystem.hpp"
#include <string>

class DisplaySystem;

enum class JPGDecodeMode {
    HARDWARE,  // Use ESP-IDF hardware decoder
    SOFTWARE   // Use esp_new_jpeg block decoder
};

class JPGImage {
public:
    JPGImage() = default;
    ~JPGImage() = default;

    auto readFromBytes(uint8_t *jpgbytes, int size, JPGDecodeMode mode = JPGDecodeMode::HARDWARE) -> bool;
    auto load(const std::string &filePath, FileSystem &fs) -> bool;
    auto getBitsPerPixel() -> int;
    auto getBuffer() -> uint8_t *;
    auto getWidth() const -> int { return width_; }
    auto getHeight() const -> int { return height_; }
    void draw(DisplaySystem &display, int x, int y);

private:
    // Existing hardware decode method (renamed)
    auto readFromBytesHardware(uint8_t *jpgbytes, int size) -> bool;
    
    // New software decode method
    auto readFromBytesSoftware(uint8_t *jpgbytes, int size) -> bool;
    
    std::shared_ptr<uint8_t[]> outputBuffer_;
    int width_ = 0;
    int height_ = 0;
    int bitsPerPixel_ = 24;
};
#endif // CONFIG_SOC_JPEG_DECODE_SUPPORTED