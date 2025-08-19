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
// For esp_new_jpeg software decoder component
#include "esp_jpeg_dec.h"
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

// Helper function for aligned allocation (16-byte alignment required for esp_new_jpeg)
static void* jpeg_calloc_align(size_t n, size_t size, size_t align) {
    void* ptr = aligned_alloc(align, n * size);
    if (ptr) {
        memset(ptr, 0, n * size);
    }
    return ptr;
}

// New software decoding implementation using esp_new_jpeg
auto JPGImage::readFromBytesSoftware(uint8_t *jpgbytes, int size) -> bool {
#if !SIMULATOR && CONFIG_SOC_JPEG_DECODE_SUPPORTED
    log_d("JPGImage::readFromBytesSoftware using esp_new_jpeg decoder with DSI protection, size: %d bytes", size);

    // Configure decoder for BGR output (to match display expectations)
    jpeg_dec_config_t config;
    config.output_type = JPEG_PIXEL_FORMAT_RGB888;  // Still RGB888 format
    config.scale = {0, 0};  // No scaling (width=0, height=0 means no scaling)
    config.clipper = {0, 0}; // No clipping  
    config.rotate = JPEG_ROTATE_0D;
    config.block_enable = false;  // Single decode call

    jpeg_dec_handle_t decoder;
    jpeg_error_t ret = jpeg_dec_open(&config, &decoder);
    if (ret != JPEG_ERR_OK) {
        log_e("jpeg_dec_open failed: %d", ret);
        return false;
    }

    // Setup input I/O structure
    jpeg_dec_io_t io;
    io.inbuf = jpgbytes;
    io.inbuf_len = size;
    io.inbuf_remain = size;
    io.outbuf = nullptr;  // Will be set after header parsing
    io.out_size = 0;      // Will be calculated after header parsing

    // Parse JPEG header to get image dimensions
    jpeg_dec_header_info_t header_info;
    ret = jpeg_dec_parse_header(decoder, &io, &header_info);
    if (ret != JPEG_ERR_OK) {
        log_e("jpeg_dec_parse_header failed: %d", ret);
        jpeg_dec_close(decoder);
        return false;
    }

    width_ = header_info.width;
    height_ = header_info.height;
    log_d("JPEG header parsed: %dx%d", width_, height_);

    // Calculate output buffer size (RGB888 = 3 bytes per pixel)
    size_t output_size = width_ * height_ * 3;
    
    // Allocate 16-byte aligned buffer using helper function
    uint8_t* aligned_buffer = (uint8_t*)jpeg_calloc_align(1, output_size, 16);
    if (!aligned_buffer) {
        log_e("Failed to allocate aligned output buffer for esp_new_jpeg decode");
        jpeg_dec_close(decoder);
        return false;
    }

    outputBuffer_ = std::shared_ptr<uint8_t[]>(aligned_buffer, [](uint8_t *ptr) { 
        free(ptr); 
    });

    // Update I/O structure with output buffer
    io.outbuf = aligned_buffer;
    io.out_size = output_size;

    // Process JPEG decode with yielding for DSI underflow prevention
    log_d("Starting JPEG decode with DSI protection...");
    
    // Add small delay before decode
    vTaskDelay(pdMS_TO_TICKS(2));
    
    // Single decode call (block mode disabled for debugging)
    ret = jpeg_dec_process(decoder, &io);
    
    if (ret != JPEG_ERR_OK) {
        log_e("jpeg_dec_process failed: %d", ret);
        jpeg_dec_close(decoder);
        return false;
    }
    
    // Add delay after decode to let DSI catch up
    vTaskDelay(pdMS_TO_TICKS(2));

    // Convert RGB to BGR to match display expectations
    log_d("Converting RGB to BGR for display compatibility...");
    uint8_t* rgb_data = aligned_buffer;
    for (size_t i = 0; i < output_size; i += 3) {
        // Swap R and B channels: RGB -> BGR
        uint8_t temp = rgb_data[i];     // Save R
        rgb_data[i] = rgb_data[i + 2];  // R = B
        rgb_data[i + 2] = temp;         // B = R
        // G channel (i + 1) stays in place
    }

    // Cleanup
    jpeg_dec_close(decoder);

    log_d("JPEG software decoded successfully via esp_new_jpeg: %dx%d, total size: %zu bytes (BGR)", 
          width_, height_, output_size);
    return true;

#else
    log_i("JPG software decoding not available (use JPGImageSTB for simulator)");
    return false;
#endif // CONFIG_SOC_JPEG_DECODE_SUPPORTED
}

auto JPGImage::getBitsPerPixel() -> int { return bitsPerPixel_; }

auto JPGImage::getBuffer() -> uint8_t * { return outputBuffer_.get(); }
#endif // CONFIG_SOC_JPEG_DECODE_SUPPORTED