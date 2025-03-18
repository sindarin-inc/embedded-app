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
enum class PixelResolution : uint8_t { ONE_BIT, EIGHT_BITS, SIXTEEN_BITS, TWENTYFOUR_BITS };

// For 8-bit screen:
//
//   - the Screen Resolution can be EIGHT_BITS or ONE_BIT, depending on their availability with the
//     device
//   - if CONFIG_FONT_TTF: the Font Resolution can be EIGHT_BITS (grayscale antialiasing) or
//     ONE_BIT (Monochome)
//   - if IMBF_SUPPORT: only ONE_BIT is available for Font Resolution
//
// For the first version of the Sol Reader, only ONE_BIT is available for both Display Screen and
// Font resolution, for both CONFIG_FONT_IBMF and CONFIG_FONT_TTF

#if SIMULATOR
#if CONFIG_DISPLAY_SIM_16BIT
const constexpr PixelResolution DEFAULT_DISPLAY_PIXEL_RESOLUTION = PixelResolution::SIXTEEN_BITS;
const constexpr PixelResolution DEFAULT_FONT_PIXEL_RESOLUTION = PixelResolution::SIXTEEN_BITS;
#elif CONFIG_DISPLAY_SIM_24BIT
const constexpr PixelResolution DEFAULT_DISPLAY_PIXEL_RESOLUTION = PixelResolution::TWENTYFOUR_BITS;
const constexpr PixelResolution DEFAULT_FONT_PIXEL_RESOLUTION = PixelResolution::TWENTYFOUR_BITS;
#elif CONFIG_DISPLAY_SIM_8BIT
const constexpr PixelResolution DEFAULT_DISPLAY_PIXEL_RESOLUTION = PixelResolution::EIGHT_BITS;
const constexpr PixelResolution DEFAULT_FONT_PIXEL_RESOLUTION = PixelResolution::EIGHT_BITS;
#else
const constexpr PixelResolution DEFAULT_DISPLAY_PIXEL_RESOLUTION = PixelResolution::ONE_BIT;
const constexpr PixelResolution DEFAULT_FONT_PIXEL_RESOLUTION = PixelResolution::ONE_BIT;
#endif
#else
#if CONFIG_DISPLAY_ESP32_P4_SEEYA_049
const constexpr PixelResolution DEFAULT_DISPLAY_PIXEL_RESOLUTION = PixelResolution::TWENTYFOUR_BITS;
const constexpr PixelResolution DEFAULT_FONT_PIXEL_RESOLUTION = PixelResolution::TWENTYFOUR_BITS;
#elif CONFIG_DISPLAY_ESP32_P4_FUNCTION_EV || CONFIG_DISPLAY_ESP32_P4_FUNCTION_EV_R0
const constexpr PixelResolution DEFAULT_DISPLAY_PIXEL_RESOLUTION = PixelResolution::SIXTEEN_BITS;
const constexpr PixelResolution DEFAULT_FONT_PIXEL_RESOLUTION = PixelResolution::SIXTEEN_BITS;
#elif CONFIG_WAVESHARE_8BIT
const constexpr PixelResolution DEFAULT_DISPLAY_PIXEL_RESOLUTION = PixelResolution::EIGHT_BITS;
const constexpr PixelResolution DEFAULT_FONT_PIXEL_RESOLUTION = PixelResolution::EIGHT_BITS;
#else
const constexpr PixelResolution DEFAULT_DISPLAY_PIXEL_RESOLUTION = PixelResolution::ONE_BIT;
const constexpr PixelResolution DEFAULT_FONT_PIXEL_RESOLUTION = PixelResolution::ONE_BIT;
#endif
#endif

const constexpr bool PIXEL_RESOLUTION_OK =
    ((DEFAULT_DISPLAY_PIXEL_RESOLUTION == PixelResolution::SIXTEEN_BITS) ||
     (DEFAULT_DISPLAY_PIXEL_RESOLUTION == PixelResolution::EIGHT_BITS) ||
     (DEFAULT_DISPLAY_PIXEL_RESOLUTION == PixelResolution::TWENTYFOUR_BITS) ||
     (DEFAULT_FONT_PIXEL_RESOLUTION == PixelResolution::ONE_BIT));

#if CONFIG_FONT_IBMF
static_assert(DEFAULT_FONT_PIXEL_RESOLUTION == PixelResolution::ONE_BIT,
              "For CONFIG_FONT_IBMF, the DEFAULT_FONT_PIXEL_RESOLUTION must be equal to ONE_BIT!");
#endif

#if !SIMULATOR && !CONFIG_WAVESHARE_8BIT && !CONFIG_DISPLAY_ESP32_P4_FUNCTION_EV &&                \
    !CONFIG_DISPLAY_ESP32_P4_FUNCTION_EV_R0 && !CONFIG_DISPLAY_ESP32_P4_SEEYA_049
static_assert(DEFAULT_DISPLAY_PIXEL_RESOLUTION == PixelResolution::ONE_BIT,
              "The first version of Sol Reader only supports an ONE_BIT display!");
static_assert(DEFAULT_FONT_PIXEL_RESOLUTION == PixelResolution::ONE_BIT,
              "The first version of Sol Reader only supports ONE_BIT font!");
#endif

static_assert(
    PIXEL_RESOLUTION_OK,
    "The font pixel resolution cannot be EIGHT_BITS if the display resolution is not EIGHT_BITS!");

// FIX16 is a floating point value in 16 bits fixed point notation, 6 bits of fraction
typedef int16_t FIX16;

struct Bitmap {
    MemoryPtr pixels;
    Dim dim;
    uint16_t pitch;
    void clear() {
        pixels = nullptr;
        dim = Dim(0, 0);
        pitch = 0;
    }
};
typedef Bitmap *BitmapPtr;

struct GlyphMetrics {
    int16_t xoff, yoff;
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