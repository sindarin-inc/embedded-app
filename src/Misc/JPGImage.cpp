#if USE_JPG_DECODER
#include "JPGImage.hpp"

#include <cstring>

#include "sindarin-debug.h"
#if !SIMULATOR
#include <jpeg_decoder.h>
#endif

auto JPGImage::readFromBytes(uint8_t *jpgbytes, int size) -> bool {
#if !SIMULATOR
    esp_jpeg_image_cfg_t jpegCfg;
    memset(&jpegCfg, 0, sizeof(jpegCfg));
    jpegCfg.indata = jpgbytes;
    jpegCfg.indata_size = size;
    jpegCfg.outbuf = outputBuffer_.get();
    jpegCfg.outbuf_size = 256 * 256 * 3;
    jpegCfg.out_format = JPEG_IMAGE_FORMAT_RGB888;
    jpegCfg.out_scale = JPEG_IMAGE_SCALE_0;
    jpegCfg.flags.swap_color_bytes = 1;

    esp_jpeg_image_output_t outimg;
    esp_err_t err = esp_jpeg_decode(&jpegCfg, &outimg);
    if (err != ESP_OK) {
        log_e("error decoding jpeg image: %s", esp_err_to_name(err));
        return false;
    }
#else
    log_i("TBD: jpg decoding for simulator");
#endif

    return true;
}

auto JPGImage::getBitsPerPixel() -> int { return 24; }

auto JPGImage::getBuffer() -> uint8_t * { return outputBuffer_.get(); }
#endif