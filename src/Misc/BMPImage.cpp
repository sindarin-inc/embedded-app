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
    if (bitcount != 8 && bitcount != 1) {
        log_e("%d bit bmp not yet supported", bitcount);
        return false;
    }

    bitsPerPixel_ = bitcount;

    int outputBytesPerLine;
    if (bitsPerPixel_ == 8) {
        outputBytesPerLine = bytesPerLine;
    } else {
        outputBytesPerLine = (width_ + 7) / 8;
    }

    // BMP stores lines of pixels starting at the bottom
    // copy starting at the bottom to match current bitmap format
    uint8_t* imptr;
    uint8_t* buf = outputBuffer_.get();
    for (int y = height_-1; y >= 0; y--) {
        imptr = &img->ciPixelArray[y*bytesPerLine];
        memcpy(buf, imptr, outputBytesPerLine);

        // bit convert to match xbm format
        // TODO: remove this for pbm bitmap format
        if (bitsPerPixel_ == 1) {
            uint8_t tmpin, tmpout;
            for (int x = 0; x < width_; x++) {
                tmpin = buf[x];
                tmpout = 0;
                for (int b = 0; b < 8; b++) {
                    tmpout <<= 1;
                    tmpout |= tmpin & 0b1;
                    tmpin >>= 1;
                }

                // flip color bits
                tmpout ^= 0xFF;
                buf[x] = tmpout;
            }
        }

        buf += outputBytesPerLine;
    }

    BmpCleanup(nullptr, img);

    return true;
}

auto BMPImage::getBitsPerPixel() -> int { return bitsPerPixel_; }

auto BMPImage::getBuffer() -> uint8_t * { return outputBuffer_.get(); }

auto BMPImage::getWidth() -> int { return width_; }

auto BMPImage::getHeight() -> int { return height_; }
