#include "JPGImage.hpp"

#include <string.h>

#include "sindarin-debug.h"
#if !SIMULATOR
#include <jpeg_decoder.h>
#endif

bool JPGImage::readFromBytes(uint8_t *jpgbytes, int size) {
#if !SIMULATOR
    esp_jpeg_image_cfg_t jpeg_cfg;
    memset(&jpeg_cfg, 0, sizeof(jpeg_cfg));
    jpeg_cfg.indata = jpgbytes;
    jpeg_cfg.indata_size = size;
    jpeg_cfg.outbuf = outputBuffer_.get();
    jpeg_cfg.outbuf_size = 256 * 256 * 3;
    jpeg_cfg.out_format = JPEG_IMAGE_FORMAT_RGB888;
    jpeg_cfg.out_scale = JPEG_IMAGE_SCALE_0;
    jpeg_cfg.flags.swap_color_bytes = 1;

    esp_jpeg_image_output_t outimg;
    esp_err_t err = esp_jpeg_decode(&jpeg_cfg, &outimg);
    if (err != ESP_OK) {
        log_e("error decoding jpeg image: %s", esp_err_to_name(err));
        return false;
    }
#else
    log_i("TBD: jpg decoding for simulator");
#endif

    return true;
}

int JPGImage::getBytesPerPixel() { return 3; }

uint8_t *JPGImage::getBuffer() { return outputBuffer_.get(); }