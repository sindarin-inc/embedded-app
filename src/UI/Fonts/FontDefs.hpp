#pragma once

#include <inttypes.h>

// These are the definitions that are common to both IBMF and TTF Font types

namespace font_defs {

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

typedef uint8_t *MemoryPtr;
enum class PixelResolution : uint8_t { ONE_BIT, EIGHT_BITS };

#if DISPLAY_SIM_8BIT
const constexpr PixelResolution DEFAULT_RESOLUTION = PixelResolution::EIGHT_BITS;
#else
const constexpr PixelResolution DEFAULT_RESOLUTION = PixelResolution::ONE_BIT;
#endif

// FIX16 is a floating point value in 16 bits fixed point notation, 6 bits of fraction
typedef int16_t FIX16;

struct Bitmap {
    MemoryPtr pixels;
    Dim dim;
    void clear() {
        pixels = nullptr;
        dim = Dim(0, 0);
    }
};
typedef Bitmap *BitmapPtr;

struct GlyphMetrics {
    int16_t xoff, yoff; // Used when the glyph is retrieved for caching
    int16_t descent;
    FIX16 advance;      // Normal advance to the next glyph position in line
    int16_t lineHeight; // This is the normal line height for all glyphs in the face
    void clear() {
        xoff = yoff = 0;
        advance = lineHeight = 0;
    }
};

struct Glyph {
    GlyphMetrics metrics;
    Bitmap bitmap;
    uint8_t pointSize;
    void clear() {
        metrics.clear();
        bitmap.clear();
        pointSize = 0;
    }
};

typedef uint16_t GlyphCode;

const constexpr GlyphCode DONT_CARE_CODE = 0x7FFC;
const constexpr GlyphCode ZERO_WIDTH_CODE = 0x7FFD;
const constexpr GlyphCode SPACE_CODE = 0x7FFE;
const constexpr GlyphCode NO_GLYPH_CODE = 0x7FFF;

const constexpr char32_t ZERO_WIDTH_CODEPOINT = 0xFEFF; // U+0FEFF
const constexpr char32_t UNKNOWN_CODEPOINT = 0xE05E;    // U+E05E This is part of the Sol Font.

} // namespace font_defs