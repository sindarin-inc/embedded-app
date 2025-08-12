#ifdef SIMULATOR

#include "../../lib/stb_image/stb_image.h"
#include "JPGImageSTB.hpp"
#include "Misc/Ditherer.hpp"
#include "sindarin-debug.h"
#include <cstring>

JPGImageSTB::JPGImageSTB() : outputBuffer_(nullptr) {}

auto JPGImageSTB::load(const std::string &filePath, FileSystem &fs) -> bool {
    // Try to open the file directly - exists() seems to have issues
    log_d("Attempting to open JPG file: %s", filePath.c_str());
    
    SFile file = fs.open(filePath);
    if (!file) {
        log_e("Failed to open JPG file: %s", filePath.c_str());
        return false;
    }
    
    log_d("Successfully opened JPG file: %s", filePath.c_str());
    
    // Read file into buffer
    file.seek(0, SSeekEnd);
    size_t fileSize = file.size();
    file.seek(0, SSeekSet);
    
    log_d("JPG file size: %zu bytes", fileSize);
    
    auto fileBuffer = std::make_unique<uint8_t[]>(fileSize);
    size_t bytesRead = file.read(fileBuffer.get(), fileSize);
    
    log_d("Read %zu bytes from JPG file", bytesRead);
    
    if (bytesRead != fileSize) {
        log_e("Failed to read complete JPG file: expected %zu, got %zu", fileSize, bytesRead);
        return false;
    }
    
    return loadFromBytes(fileBuffer.get(), static_cast<int>(fileSize));
}

auto JPGImageSTB::loadFromBytes(const uint8_t *jpgData, int size) -> bool {
    int channels;
    
    log_d("Attempting to load JPG from %d bytes of data", size);
    
    // Load image using STB - force to grayscale (1 channel)
    unsigned char *data = stbi_load_from_memory(jpgData, size, &width_, &height_, &channels, 1);
    if (!data) {
        log_e("STB failed to load JPG: %s", stbi_failure_reason());
        return false;
    }
    
    log_d("Successfully loaded JPG: %dx%d, original channels: %d", width_, height_, channels);
    
    // Scale down large images to avoid memory issues (max 1024x1024)
    const int MAX_DIMENSION = 1024;
    if (width_ > MAX_DIMENSION || height_ > MAX_DIMENSION) {
        float scale = std::min((float)MAX_DIMENSION / width_, (float)MAX_DIMENSION / height_);
        int newWidth = (int)(width_ * scale);
        int newHeight = (int)(height_ * scale);
        
        log_d("Scaling image from %dx%d to %dx%d", width_, height_, newWidth, newHeight);
        
        // Create scaled image buffer
        auto scaledData = std::make_unique<uint8_t[]>(newWidth * newHeight);
        
        // Simple nearest-neighbor scaling
        for (int y = 0; y < newHeight; y++) {
            for (int x = 0; x < newWidth; x++) {
                int srcX = (int)(x / scale);
                int srcY = (int)(y / scale);
                scaledData[y * newWidth + x] = data[srcY * width_ + srcX];
            }
        }
        
        // Replace original data with scaled data
        stbi_image_free(data);
        data = scaledData.release();
        width_ = newWidth;
        height_ = newHeight;
    }
    
#if CONFIG_DISPLAY_SIM_8BIT
    // For 8-bit simulator display, keep as 8-bit grayscale
    int outputBytesPerLine = width_;
    int outputTotalBytes = outputBytesPerLine * height_;
    
    log_d("Allocating %d bytes for 8-bit grayscale buffer", outputTotalBytes);
    
    outputBuffer_ = std::shared_ptr<uint8_t[]>(new uint8_t[outputTotalBytes], std::default_delete<uint8_t[]>());
    bitsPerPixel_ = 8;
    
    // Just copy the grayscale data directly
    memcpy(outputBuffer_.get(), data, outputTotalBytes);
#else
    // Convert to 1-bit bitmap for e-ink display using dithering
    int outputBytesPerLine = (width_ + 7) / 8;
    int outputTotalBytes = outputBytesPerLine * height_;
    
    log_d("Allocating %d bytes for 1-bit output buffer (%d bytes per line)", outputTotalBytes, outputBytesPerLine);
    
    outputBuffer_ = std::shared_ptr<uint8_t[]>(new uint8_t[outputTotalBytes], std::default_delete<uint8_t[]>());
    bitsPerPixel_ = 1;
    
    // Use the ditherer to convert grayscale to 1-bit
    // Parameters: input data, output buffer, width, height, input bytes per line, reversed
    // STB image gives us packed grayscale data (1 byte per pixel), so bytesPerLine = width
    Ditherer::getInstance().ditherToBitmap(data, outputBuffer_.get(), width_, height_, width_, true);
#endif
    
    log_d("JPG conversion completed successfully (%d-bit)", bitsPerPixel_);
    
    stbi_image_free(data);
    return true;
}

void JPGImageSTB::draw(DisplaySystem &display, int x, int y) {
    if (outputBuffer_ == nullptr) {
        log_e("JPG not loaded, cannot draw");
        return;
    }
    
#if CONFIG_DISPLAY_SIM_8BIT
    // For 8-bit display, draw pixel by pixel since drawBitmap expects 1-bit data
    display.gfxClass()->startWrite();
    for (int py = 0; py < height_; py++) {
        for (int px = 0; px < width_; px++) {
            uint8_t grayValue = outputBuffer_[py * width_ + px];
            // Invert the grayscale value since it appears to be inverted
            grayValue = 255 - grayValue;
            // Convert 8-bit grayscale (0-255) to RGB565 grayscale
            // Map grayscale to RGB565: gray = (gray >> 2) << 11 | (gray >> 1) << 5 | (gray >> 2)
            uint16_t gray5 = grayValue >> 3;  // 8-bit to 5-bit
            uint16_t gray6 = grayValue >> 2;  // 8-bit to 6-bit  
            uint16_t color = (gray5 << 11) | (gray6 << 5) | gray5;
            display.gfxClass()->writePixel(x + px, y + py, color);
        }
    }
    display.gfxClass()->endWrite();
    log_d("Drew 8-bit JPG image pixel by pixel: %dx%d", width_, height_);
#else
    // For 1-bit display, use the regular bitmap method
    display.drawBitmap(x, y, outputBuffer_.get(), width_, height_);
#endif
}

#endif