#pragma once

#include <inttypes.h>

struct Dim {
    int16_t width;
    int16_t height;
    Dim(uint16_t w, uint16_t h) : width(w), height(h) {}
    Dim() = default;
};

struct Pos {
    int16_t x;
    int16_t y;
    Pos(int16_t xpos, int16_t ypos) : x(xpos), y(ypos) {}
    Pos() = default;
};

enum class PixelResolution : uint8_t { ONE_BIT, EIGHT_BITS };

struct Bitmap {
    MemoryPtr pixels;
    Dim dim;
    void clear() {
        pixels = nullptr;
        dim = Dim(0, 0);
    }
};
typedef Bitmap *BitmapPtr;