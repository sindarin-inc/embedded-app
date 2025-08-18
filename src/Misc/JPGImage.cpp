#if CONFIG_SOC_JPEG_DECODE_SUPPORTED
#include "JPGImage.hpp"

#include <algorithm>
#include <cstdlib>
#include <cstring>

#include "Displays/DisplaySystem.hpp"
#include "Renderers/RendererStream.hpp"
#include "sindarin-debug.h"

#if !SIMULATOR && CONFIG_SOC_JPEG_DECODE_SUPPORTED
#include "driver/jpeg_decode.h"
// For esp_jpeg software decoder component (if available)
#ifdef CONFIG_ESP_JPEG_COMPONENT
#include "esp_jpeg_dec.h"
#endif
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
    return readFromBytes(fileBuffer.get(), static_cast<int>(fileSize), JPGDecodeMode::SOFTWARE);
}

auto JPGImage::readFromBytes(uint8_t *jpgbytes, int size, JPGDecodeMode mode) -> bool {
    switch (mode) {
    case JPGDecodeMode::HARDWARE:
        return readFromBytesHardware(jpgbytes, size);
    case JPGDecodeMode::SOFTWARE:
        return readFromBytesSoftware(jpgbytes, size);
    default:
        log_e("Unknown JPEG decode mode");
        return false;
    }
}

auto JPGImage::readFromBytesHardware(uint8_t *jpgbytes, int size) -> bool {
#if !SIMULATOR && CONFIG_SOC_JPEG_DECODE_SUPPORTED
    // Use modern ESP-IDF JPEG decoder API like focus-firmware
    jpeg_decode_engine_cfg_t engine_cfg = {
        .intr_priority = 3,
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
        .rgb_order = JPEG_DEC_RGB_ELEMENT_ORDER_BGR,
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

    memcpy(display.getBuffer(), outputBuffer_.get(), width_ * height_ * 3);

    log_d("Drew JPG image: %dx%d", width_, height_);
}

// New software decoding implementation
auto JPGImage::readFromBytesSoftware(uint8_t *jpgbytes, int size) -> bool {
#if !SIMULATOR && CONFIG_SOC_JPEG_DECODE_SUPPORTED

#ifdef CONFIG_ESP_JPEG_COMPONENT
    // Use esp_jpeg component for software decode (if available)
    log_d("JPGImage::readFromBytesSoftware using esp_jpeg component, size: %d bytes", size);

    // First pass: get image info without allocating output buffer
    esp_jpeg_image_cfg_t info_cfg = {.indata = jpgbytes,
                                     .indata_size = size,
                                     .outbuf = nullptr, // No output buffer for info-only decode
                                     .outbuf_size = 0,
                                     .out_format = JPEG_IMAGE_FORMAT_RGB888,
                                     .out_scale = JPEG_IMAGE_SCALE_0,
                                     .flags = {
                                         .swap_color_bytes = 0,
                                     }};

    esp_jpeg_image_output_t outimg_info;
    esp_err_t ret = esp_jpeg_decode(&info_cfg, &outimg_info);

    if (ret != ESP_OK) {
        log_e("esp_jpeg_decode info pass failed: %s", esp_err_to_name(ret));
        return false;
    }

    width_ = outimg_info.width;
    height_ = outimg_info.height;
    log_d("JPEG info: %dx%d", width_, height_);

    // Calculate output buffer size
    size_t output_size = width_ * height_ * 3; // RGB888

    // Allocate output buffer with 16-byte alignment
    uint8_t *aligned_buffer = (uint8_t *)aligned_alloc(16, output_size);
    if (!aligned_buffer) {
        // Fallback to regular malloc if aligned_alloc not available
        aligned_buffer = (uint8_t *)malloc(output_size);
    }

    if (!aligned_buffer) {
        log_e("Failed to allocate output buffer for software decode");
        return false;
    }

    outputBuffer_ = std::shared_ptr<uint8_t[]>(aligned_buffer, [](uint8_t *ptr) { free(ptr); });

    // Second pass: actual decode with output buffer
    esp_jpeg_image_cfg_t decode_cfg = {.indata = jpgbytes,
                                       .indata_size = size,
                                       .outbuf = aligned_buffer,
                                       .outbuf_size = output_size,
                                       .out_format = JPEG_IMAGE_FORMAT_RGB888,
                                       .out_scale = JPEG_IMAGE_SCALE_0,
                                       .flags = {
                                           .swap_color_bytes = 0,
                                       }};

    esp_jpeg_image_output_t outimg;
    ret = esp_jpeg_decode(&decode_cfg, &outimg);

    if (ret != ESP_OK) {
        log_e("esp_jpeg_decode failed: %s", esp_err_to_name(ret));
        return false;
    }

    log_d("JPEG software decoded successfully: %dx%d, total size: %zu bytes", width_, height_,
          output_size);
    return true;

#else
    // Fallback: Use hardware decode with yielding for DSI underflow prevention
    log_d("JPGImage::readFromBytesSoftware using hardware decode with yielding, size: %d bytes",
          size);

    // Add small delay before hardware decode to prevent DSI underflow
    vTaskDelay(pdMS_TO_TICKS(2));

    bool result = readFromBytesHardware(jpgbytes, size);

    // Add small delay after decode to allow DSI to catch up
    if (result) {
        vTaskDelay(pdMS_TO_TICKS(2));
        log_d("Hardware decode with yielding completed successfully");
    }

    return result;
#endif

#else
    log_i("JPG software decoding not available (use JPGImageSTB for simulator)");
    return false;
#endif // CONFIG_SOC_JPEG_DECODE_SUPPORTED
}

auto JPGImage::getBitsPerPixel() -> int { return bitsPerPixel_; }

auto JPGImage::getBuffer() -> uint8_t * { return outputBuffer_.get(); }
#endif // CONFIG_SOC_JPEG_DECODE_SUPPORTED