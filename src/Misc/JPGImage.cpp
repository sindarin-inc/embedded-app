#if CONFIG_SOC_JPEG_DECODE_SUPPORTED
#include "JPGImage.hpp"

#include <cstring>

#include "Displays/DisplaySystem.hpp"
#include "Renderers/RendererStream.hpp"
#include "sindarin-debug.h"

#if !SIMULATOR && CONFIG_SOC_JPEG_DECODE_SUPPORTED
#include "driver/jpeg_decode.h"
#endif // CONFIG_SOC_JPEG_DECODE_SUPPORTED

auto JPGImage::load(const std::string &filePath, FileSystem &fs) -> bool {
    log_d("JPGImage::load attempting to load: %s", filePath.c_str());

    if (!fs.exists(filePath)) {
        log_e("JPG file not found: %s", filePath.c_str());
        log_d("FileSystem type: %s", typeid(fs).name());
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

    file.close();
    return readFromBytes(fileBuffer.get(), static_cast<int>(fileSize));
}

auto JPGImage::readFromBytes(uint8_t *jpgbytes, int size) -> bool {
#if !SIMULATOR && CONFIG_SOC_JPEG_DECODE_SUPPORTED
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

    // Check if image is too large and needs scaling down for memory constraints
    const int MAX_DIMENSION = 800; // Reasonable limit for device memory
    if (width_ > MAX_DIMENSION || height_ > MAX_DIMENSION) {
        log_w("Image %dx%d is too large, may cause memory issues. Consider scaling down.", width_,
              height_);
        // For now, continue but be aware this might fail
    }

    // Calculate aligned dimensions (decoder aligns to 16 bytes automatically)
    int aligned_width = (width_ + 15) & ~15;   // Round up to nearest 16
    int aligned_height = (height_ + 15) & ~15; // Round up to nearest 16
    size_t output_size = aligned_width * aligned_height * 3;
    log_d("Aligned dimensions: %dx%d, buffer size: %zu", aligned_width, aligned_height,
          output_size);
    jpeg_decode_memory_alloc_cfg_t mem_cfg = {JPEG_DEC_ALLOC_OUTPUT_BUFFER};
    size_t allocated_size;
    uint8_t *aligned_buffer =
        (uint8_t *)jpeg_alloc_decoder_mem(output_size, &mem_cfg, &allocated_size);
    if (!aligned_buffer) {
        log_e("Failed to allocate aligned output buffer");
        jpeg_del_decoder_engine(decoder_engine);
        return false;
    }

    // Wrap in shared_ptr with custom deleter
    outputBuffer_ = std::shared_ptr<uint8_t[]>(aligned_buffer, [](uint8_t *ptr) { free(ptr); });
    log_d("Allocated %zu bytes (requested %zu) for aligned JPEG output buffer", allocated_size,
          output_size);

    // Decode the image
    jpeg_decode_cfg_t decode_cfg = {
        .output_format = JPEG_DECODE_OUT_FORMAT_RGB888,
        .rgb_order = JPEG_DEC_RGB_ELEMENT_ORDER_RGB,
    };

    uint32_t out_size;
    ret = jpeg_decoder_process(decoder_engine, &decode_cfg, jpgbytes, size, outputBuffer_.get(),
                               allocated_size, &out_size);

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
#endif // CONFIG_SOC_JPEG_DECODE_SUPPORTED
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
#endif // CONFIG_SOC_JPEG_DECODE_SUPPORTED