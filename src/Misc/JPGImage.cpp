#if USE_JPG_DECODER
#include "JPGImage.hpp"

#include <cstring>
#include "sindarin-debug.h"
#include "Displays/DisplaySystem.hpp"
#include "Renderers/RendererStream.hpp"

#if !SIMULATOR
#include "driver/jpeg_decode.h"
#endif

auto JPGImage::load(const std::string &filePath, FileSystem &fs) -> bool {
    if (!fs.exists(filePath)) {
        log_e("JPG file not found: %s", filePath.c_str());
        return false;
    }
    
    SFile file = fs.open(filePath);
    if (!file) {
        log_e("Failed to open JPG file: %s", filePath.c_str());
        return false;
    }
    
    // Read file into buffer
    file.seek(0, SSeekEnd);
    size_t fileSize = file.size();
    file.seek(0, SSeekSet);
    
    log_d("JPG file size: %zu bytes", fileSize);
    
    auto fileBuffer = std::make_unique<uint8_t[]>(fileSize);
    size_t bytesRead = file.read(fileBuffer.get(), fileSize);
    
    if (bytesRead != fileSize) {
        log_e("Failed to read complete JPG file: expected %zu, got %zu", fileSize, bytesRead);
        return false;
    }
    
    return readFromBytes(fileBuffer.get(), static_cast<int>(fileSize));
}

auto JPGImage::readFromBytes(uint8_t *jpgbytes, int size) -> bool {
#if !SIMULATOR
    // Use modern ESP-IDF JPEG decoder API like focus-firmware
    jpeg_decode_engine_cfg_t engine_cfg = {
        .intr_priority = 0,
        .timeout_ms = 40,
    };
    
    jpeg_decoder_handle_t decoder_engine;
    esp_err_t ret = jpeg_new_decoder_engine(&engine_cfg, &decoder_engine);
    if (ret != ESP_OK) {
        log_e("JPEG decoder engine create failed: %s", esp_err_to_name(ret));
        return false;
    }
    
    // Get image info first
    jpeg_decode_picture_info_t picture_info;
    ret = jpeg_decoder_get_info(jpgbytes, size, &picture_info);
    if (ret != ESP_OK) {
        log_e("JPEG get info failed: %s", esp_err_to_name(ret));
        jpeg_del_decoder_engine(decoder_engine);
        return false;
    }
    
    width_ = picture_info.width;
    height_ = picture_info.height;
    log_d("JPEG info: %dx%d", width_, height_);
    
    // Allocate output buffer (RGB888 format)
    size_t output_size = width_ * height_ * 3;
    outputBuffer_ = std::shared_ptr<uint8_t[]>((uint8_t *)SpiRamMalloc(output_size), free);
    if (!outputBuffer_) {
        log_e("Failed to allocate output buffer");
        jpeg_del_decoder_engine(decoder_engine);
        return false;
    }
    
    // Decode the image
    jpeg_decode_cfg_t decode_cfg = {
        .output_format = JPEG_DECODE_OUT_FORMAT_RGB888,
        .rgb_order = JPEG_DEC_RGB_ELEMENT_ORDER_RGB,
    };
    
    uint32_t out_size;
    ret = jpeg_decoder_process(decoder_engine, &decode_cfg, jpgbytes, size, 
                              outputBuffer_.get(), output_size, &out_size);
    
    jpeg_del_decoder_engine(decoder_engine);
    
    if (ret != ESP_OK) {
        log_e("JPEG decode process failed: %s", esp_err_to_name(ret));
        return false;
    }
    
    log_d("JPEG decoded successfully: %dx%d, output size: %lu bytes", width_, height_, out_size);
    return true;
    
#else
    log_i("JPG decoding not implemented for simulator (use JPGImageSTB instead)");
    return false;
#endif
}

void JPGImage::draw(DisplaySystem &display, int x, int y) {
    if (outputBuffer_ == nullptr) {
        log_e("JPG not loaded, cannot draw");
        return;
    }
    
    // For device, the display driver should handle the RGB888 data directly
    // Just like focus-firmware does with drawBitmap
    display.gfxClass()->startWrite();
    
    // Draw RGB888 data pixel by pixel (similar to focus-firmware approach)
    uint8_t *rgb = outputBuffer_.get();
    for (int py = 0; py < height_; py++) {
        for (int px = 0; px < width_; px++) {
            uint8_t r = rgb[0];
            uint8_t g = rgb[1]; 
            uint8_t b = rgb[2];
            
            // Convert RGB888 to RGB565 for the display
            uint16_t color = ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
            display.gfxClass()->writePixel(x + px, y + py, color);
            
            rgb += 3;
        }
    }
    
    display.gfxClass()->endWrite();
    log_d("Drew JPG image: %dx%d", width_, height_);
}

auto JPGImage::getBitsPerPixel() -> int { return bitsPerPixel_; }

auto JPGImage::getBuffer() -> uint8_t * { return outputBuffer_.get(); }
#endif