#include "Ditherer.hpp"

#include <cstdio>
#include <cstdlib>

#include "Platform.hpp"
#include "sindarin-debug.h"

Ditherer ditherer;

auto Ditherer::getInstance() -> Ditherer & { return ditherer; }

void Ditherer::convertToFloat(uint8_t *image, int width, int height, int bytesPerLine,
                              bool reversed) {
    uint8_t *inpixel = image;
    float div = 1.0 / 255.0;
    float *outpixel = ditherBuffer_.get();

    if (reversed) {
        for (int h = height - 1; h >= 0; h--) {
            inpixel = &image[h * bytesPerLine];
            for (int w = 0; w < width; w++) {
                *(outpixel++) = static_cast<float>(*(inpixel++)) * div;
            }
        }
    } else {
        for (int h = 0; h < height; h++) {
            inpixel = &image[h * bytesPerLine];
            for (int w = 0; w < width; w++) {
                *(outpixel++) = static_cast<float>(*(inpixel++)) * div;
            }
        }
    }
}

void Ditherer::ditherToBitmap(uint8_t *image, uint8_t *bitmap, int width, int height,
                              int bytesPerLine, bool reversed) {
    convertToFloat(image, width, height, bytesPerLine, reversed);
    ditherFloatToBitmap(bitmap, width, height);
}

auto Ditherer::pixelAt(int x, int y, int width, int height) -> float * {
    if (x >= 0 && x < width && y >= 0 && y < height) {
        return &ditherBuffer_.get()[y * width + x];
    } else {
        return nullptr;
    }
}

void Ditherer::ditherFloatToBitmap(uint8_t *bitmap, int width, int height) {
    float *pixel = ditherBuffer_.get();
    float val;
    float div1 = 7.0 / 16.0;
    float div2 = 3.0 / 16.0;
    float div3 = 5.0 / 16.0;
    float div4 = 1.0 / 16.0;

    uint8_t *bmpptr = bitmap;
    uint8_t bmpByte = 0;
    int bitsInByte = 0;

    for (int h = 0; h < height; h++) {
        for (int w = 0; w < width; w++) {
            // clip value
            if (*pixel < 0.0) {
                *pixel = 0.0;
            } else if (*pixel > 1.0) {
                *pixel = 1.0;
            }

            // writing into xbitmap format
            // lsb is the leftmost pixel
            // TODO: change this if converting to PBM style (msb as left pixel)
            bmpByte >>= 1;
            bitsInByte++;
            if (*pixel >= 0.5) {
                val = 1.0;
            } else {
                val = 0.0;
                bmpByte |= 0x80;
            }

            float diff = *pixel - val;

            float *pp;
            pp = pixelAt(w + 1, h, width, height);
            if (pp) {
                *pp += diff * div1;
            }
            pp = pixelAt(w - 1, h + 1, width, height);
            if (pp) {
                *pp += diff * div2;
            }
            pp = pixelAt(w, h + 1, width, height);
            if (pp) {
                *pp += diff * div3;
            }
            pp = pixelAt(w + 1, h + 1, width, height);
            if (pp) {
                *pp += diff * div4;
            }

            *(pixel++) = val;
            if (bitsInByte == 8) {
                *(bmpptr++) = bmpByte;
                bitsInByte = 0;
            }
        }
    }
}