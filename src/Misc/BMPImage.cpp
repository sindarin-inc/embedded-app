#include "BMPImage.hpp"

#include "Renderers/RendererStream.hpp"
#include "bmp.h"

auto BMPImage::readFromBytes(uint8_t *bmpbytes, int size) -> bool {
    BufferStream stream(bmpbytes, size);
    bmp_image *img = BmpRead(stream);
    if (!img) {
        log_e("error reading bmp image");
        return false;
    }

    width_ = img->dib.bmiHeader.biWidth;
    height_ = img->dib.bmiHeader.biHeight;

    int datasize = img->dib.bmiHeader.biSizeImage;
    int bytesPerLine = datasize / height_;

    int bitcount = img->dib.bmiHeader.biBitCount;
    if (bitcount != 8) {
        // TODO: add support for 1 bit indexed BMP (black and white, pre-dithered)
        log_e("%d bit bmp not yet supported", bitcount);
        return false;
    }

    // BMP stores lines of pixels starting at the bottom
    // copy starting at the bottom to match current bitmap format
    uint8_t *imptr;
    uint8_t *buf = outputBuffer_.get();
    for (int y = height_ - 1; y >= 0; y--) {
        imptr = &img->ciPixelArray[y * bytesPerLine];
        memcpy(buf, imptr, width_);
        buf += width_;
    }

    BmpCleanup(nullptr, img);

    return true;
}

auto BMPImage::getBytesPerPixel() -> int { return 1; }

auto BMPImage::getBuffer() -> uint8_t * { return outputBuffer_.get(); }

auto BMPImage::getWidth() -> int { return width_; }

auto BMPImage::getHeight() -> int { return height_; }
