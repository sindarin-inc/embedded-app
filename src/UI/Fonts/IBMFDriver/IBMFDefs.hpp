#pragma once

#include <cinttypes>
#include <vector>

//---  ESP_IDF
// #include <esp_log.h>

const constexpr bool IBMF_TRACING = false;

#define OPTICAL_KERNING 1

#if OPTICAL_KERNING
const constexpr int K_BUFF_WIDTH = 40;
const constexpr int K_BUFF_HEIGHT = 25;
const constexpr int K_ORIGIN_X = 5;
const constexpr int K_ORIGIN_Y = 19;
const constexpr int KERNING_SIZE = 1;
#endif

// The following is used when testing the driver outside of this application
#if IBMF_TESTING
#define PROGMEM
#include <cstdarg>
#include <cstdio>
#include <string>

extern char *formatStr(const std::string &format, ...);

#define log_i(format, ...) std::cout << "INFO: " << formatStr(format, ##__VA_ARGS__) << std::endl;
#define log_w(format, ...)                                                                         \
    std::cout << "WARNING: " << formatStr(format, ##__VA_ARGS__) << std::endl;
#define log_e(format, ...) std::cout << "ERROR: " << formatStr(format, ##__VA_ARGS__) << std::endl;
#define log_d(format, ...) std::cout << "DEBUG: " << formatStr(format, ##__VA_ARGS__) << std::endl;
#else
#include "sindarin-debug.h"
#endif

// clang-format off
//
// The following definitions are used by all parts of the driver.
//
// The IBMF font files have the current format:
//
//  At Offset 0:
//  +--------------------+
//  |                    |  Preamble (6 bytes)
//  |                    |
//  +--------------------+
//  |                    |  Pixel sizes (one byte per face pt size present padded to 32 bits
//  |                    |  from the start) (not used by this driver)
//  +--------------------+
//  |                    |  FaceHeader offset vector 
//  |                    |  (32 bit offset for each face)
//  +--------------------+
//  |                    |  For FontFormat 1 (FontFormat::UTF32) only: the table that contains corresponding 
//  |                    |  values between Unicode CodePoints and their internal GlyphCode.
//  |                    |  (content already well aligned to 32 bits frontiers)
//  +--------------------+
//
//  +--------------------+               <------------+
//  |                    |  FaceHeader                |
//  |                    |  (32 bits aligned)         |
//  |                    |                            |
//  +--------------------+                            |
//  |                    |  Glyphs' pixels indexes    |
//  |                    |  in the Pixels Pool        |
//  |                    |  (32bits each)             |
//  +--------------------+                            |
//  |                    |  GlyphsInfo                |
//  |                    |  Array (16 bits aligned)   |
//  |                    |                            |  Repeat for
//  +--------------------+                            |> each face
//  |                    |                            |  part of the
//  |                    |  Pixels Pool               |  font
//  |                    |  (No alignement, all bytes)|
//  |                    |                            |
//  +--------------------+                            |
//  |                    |  Filler (32bits padding)   |
//  +--------------------+                            |
//  |                    |                            |
//  |                    |  LigKerSteps               |
//  |                    |  (2 x 16 bits each step)   |
//  |                    |                            |
//  +--------------------+               <------------+
//             .
//             .
//             .
//
//
// clang-format on

namespace ibmf_defs {

#define LOGI(format, ...) log_i(format, ##__VA_ARGS__)
#define LOGW(format, ...) log_w(format, ##__VA_ARGS__)
#define LOGE(format, ...) log_e(format, ##__VA_ARGS__)
#define LOGD(format, ...) log_d(format, ##__VA_ARGS__)

// #define DEBUG_IBMF 1

#ifdef DEBUG_IBMF
const constexpr int DEBUG = DEBUG_IBMF;
#else
const constexpr int DEBUG = 0;
#endif

#if DEBUG_IBMF
// #include <fstream>
#include <iostream>
// #include <iomanip>
#endif

//----

const constexpr uint8_t IBMF_VERSION = 4; // Font format version
const constexpr uint8_t MAX_FACE_COUNT = 10;

const constexpr uint8_t NO_LIG_KERN_PGM = 0xFF;

// Character Sets being supported (the UTF32 is forecoming)

enum FontFormat : uint8_t { LATIN = 0, UTF32 = 1, UNKNOWN = 7 };

const constexpr uint16_t UTF32_MAX_GLYPH_COUNT = 32765; // Index Value 0xFE and 0xFF are reserved

enum class PixelResolution : uint8_t { ONE_BIT, EIGHT_BITS };

#if DISPLAY_SIM_8BIT
const constexpr PixelResolution DEFAULT_RESOLUTION = PixelResolution::EIGHT_BITS;
#else
const constexpr PixelResolution DEFAULT_RESOLUTION = PixelResolution::ONE_BIT;
#endif

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

// RLE (Run Length Encoded) Bitmap. To get something to show, they have
// to be processed through the RLEExtractor class.
// Dim contains the expected width and height once the bitmap has been
// decompressed. The length is the pixels array size in bytes.

struct RLEBitmap {
    MemoryPtr pixels;
    Dim dim;
    uint16_t length;
    void clear() {
        pixels = nullptr;
        dim = Dim(0, 0);
        length = 0;
    }
};
typedef RLEBitmap *RLEBitmapPtr;

// Uncompressed Bitmap. Depending on the resolution, every pixel will take:
//
// - one byte (PixelResolution::EIGHT_BITS)
// - one bit (PixelResolution::ONE_BIT), every byte containing 8 pixels
//   with the most significant bit being the pixel on the left of the others.
//
// The values used to represent the pixels depen on the following constants.
// Theyhave to be adjusted depending on the screen software/hardware/firmware'
// pixels polarity/color/shading/gray-scale. At least, one of BLACK...
// or WHITE... must be 0. If not, some changes are required in the code.

const constexpr uint8_t BLACK_ONE_BIT = 0;
const constexpr uint8_t WHITE_ONE_BIT = 1;

const constexpr uint8_t BLACK_EIGHT_BITS = 0;
const constexpr uint8_t WHITE_EIGHT_BITS = 0xFF;

// const constexpr uint8_t BLACK_ONE_BIT = 1;
// const constexpr uint8_t WHITE_ONE_BIT = 0;
// const constexpr uint8_t BLACK_EIGHT_BITS = 0xFF;
// const constexpr uint8_t WHITE_EIGHT_BITS = 0x00;

struct Bitmap {
    MemoryPtr pixels;
    Dim dim;
    void clear() {
        pixels = nullptr;
        dim = Dim(0, 0);
    }
};
typedef Bitmap *BitmapPtr;

#pragma pack(push, 1)

// FIX16 is a floating point value in 16 bits fixed point notation, 6 bits of fraction
// Idem for FIX14, but for 14 bits fixed point notation notation

typedef int16_t FIX16;
typedef int16_t FIX14;
typedef uint16_t GlyphCode;

struct Preamble {
    char marker[4]; // Must be "IBMF"
    uint8_t faceCount;
    struct {
        uint8_t version : 5;       // Must be IBMF_VERSION
        FontFormat fontFormat : 3; // Can be 0 (LATIN) or 1 (UTF32)
    } bits;
};
typedef Preamble *PreamblePtr;

struct FaceHeader {
    uint8_t pointSize;         // In points (pt) a point is 1 / 72.27 of an inch
    uint8_t lineHeight;        // In pixels
    uint16_t dpi;              // Pixels per inch
    FIX16 xHeight;             // Hight of character 'x' in pixels
    FIX16 emHeight;            // Hight of character 'M' in pixels
    FIX16 slantCorrection;     // When an italic face
    uint8_t descenderHeight;   // The height of the descending below the origin
    uint8_t spaceSize;         // Size of a space character in pixels
    uint16_t glyphCount;       // Must be the same for all face
    uint16_t ligKernStepCount; // Length of the Ligature/Kerning table
    uint32_t pixelsPoolSize;   // Size of the Pixels Pool
};
typedef FaceHeader *FaceHeaderPtr;
typedef uint8_t (*PixelsPoolPtr)[];
typedef uint32_t PixelPoolIndex;
typedef PixelPoolIndex (*GlyphsPixelPoolIndexes)[];

// clang-format off
//
// The lig kern array contains instructions (struct LibKernStep) in a simple programming
// language that explains what to do for special letter pairs. The information in squared
// brackets relate to fields that are part of the LibKernStep struct. Each entry in this
// array is a lig kern command of four bytes:
//
// - first byte: skip byte, indicates that this is the final program step if the byte
//                          is 128 or more, otherwise the next step is obtained by
//                          skipping this number of intervening steps [nextStepRelative].
// - second byte: next char, if next character follows the current character, then
//                           perform the operation and stop, otherwise continue.
// - third byte: op byte, indicates a ligature step if less than 128, a kern step otherwise.
// - fourth byte: remainder.
//
// In a kern step [isAKern == true], an additional space equal to kern located at
// [(displHigh << 8) + displLow] in the kern array is inserted between the current
// character and [nextChar]. This amount is often negative, so that the characters
// are brought closer together by kerning; but it might be positive.
//
// There are eight kinds of ligature steps [isAKern == false], having op byte codes
// [aOp bOp cOp] where 0 ≤ aOp ≤ bOp + cOp and 0 ≤ bOp, cOp ≤ 1.
//
// The character whose code is [replacementChar] is inserted between the current
// character and [nextChar]; then the current character is deleted if bOp = 0, and
// [nextChar] is deleted if cOp = 0; then we pass over aOp characters to reach the next
// current character (which may have a ligature/kerning program of its own).
//
// If the very first instruction of a character’s lig kern program has [whole > 128],
// the program actually begins in location [(displHigh << 8) + displLow]. This feature
// allows access to large lig kern arrays, because the first instruction must otherwise
// appear in a location ≤ 255.
//
// Any instruction with [whole > 128] in the lig kern array must have
// [(displHigh << 8) + displLow] < the size of the array. If such an instruction is
// encountered during normal program execution, it denotes an unconditional halt; no
// ligature or kerning command is performed.
//
// (The following usage has been extracted from the lig/kern array as not being used outside
//  of a TeX generated document)
//
// If the very first instruction of the lig kern array has [whole == 0xFF], the
// [nextChar] byte is the so-called right boundary character of this font; the value
// of [nextChar] need not lie between char codes boundaries.
//
// If the very last instruction of the lig kern array has [whole == 0xFF], there is
// a special ligature/kerning program for a left boundary character, beginning at location
// [(displHigh << 8) + displLow] . The interpretation is that TEX puts implicit boundary
// characters before and after each consecutive string of characters from the same font.
// These implicit characters do not appear in the output, but they can affect ligatures
// and kerning.
// -----
//
// Here is the original LigKern table entry format (4 bytes). Byte 1, 3 and 4 have
// two different meanings as show below (a big-endian format...):
//
//           Byte 1                   Byte 2
// +------------------------+------------------------+
// |        whole           |                        |
// +------------------------+       Next Char        +
// |Stop| nextStepRelative  |                        |
// +------------------------+------------------------+
//
//
//           Byte 3                   Byte 4
// +------------------------+------------------------+
// |Kern|       | a | b | c |    Replacement Char    |  <- isAKern (Kern in the diagram) is false
// +------------------------+------------------------+
// |isKern|Displacement High|    Displacement Low    |  <- isAKern is true
// +------------------------+------------------------+
//
// The following fields are not used/replaced in this application:
//
//    - nextStepRelative
//    - Ops a, b, and c
//    - whole can be reduced to one GoTo bit
//
// ----
//
// Here is the optimized version considering larger characters table
// and that some fields are not being used (BEWARE: a little-endian format):
//
//           Byte 2                   Byte 1
// +------------------------+------------------------+
// |Stop|               Next Char                    |
// +------------------------+------------------------+
//
//
//           Byte 4                   Byte 3
// +------------------------+------------------------+
// |Kern|             Replacement Char               |  <- isAKern (Kern in the diagram) is false
// +------------------------+------------------------+
// |Kern|GoTo|      Displacement in FIX14            |  <- isAKern is true and GoTo is false => Kerning value
// +------------------------+------------------------+
// |Kern|GoTo|          Displacement                 |  <- isAkern and GoTo are true
// +------------------------+------------------------+
//
// Up to 32765 different glyph codes can be managed through this format.
// Kerning displacements reduced to 14 bits is not a big issue: kernings are
// usually small numbers. FIX14 and FIX16 are using 6 bits for the fraction. Their
// remains 8 bits for FIX14 and 10 bits for FIX16, that is more than enough...
//
// This is NOW the format in use with this driver and other support apps.
//
// clang-format on

#define ORIGINAL_FORMAT 0
#if ORIGINAL_FORMAT
union SkipByte {
    uint8_t whole : 8;
    struct {
        uint8_t nextStepRelative : 7; // Not used
        bool stop : 1;
    } s;
};

union OpCodeByte {
    struct {
        bool cOp : 1;     // Not used
        bool bOp : 1;     // Not used
        uint8_t aOp : 5;  // Not used
        bool isAKern : 1; // True if Kern, False if ligature
    } op;
    struct {
        uint8_t displHigh : 7;
        bool isAKern : 1;
    } d;
};

union RemainderByte {
    SmallGlyphCode replacementChar : 8;
    uint8_t displLow : 8; // Ligature: replacement char code, kern: displacement
};

struct LigKernStep {
    SkipByte skip;
    SmallGlyphCode nextChar;
    OpCodeByte opCode;
    RemainderByte remainder;
};
#else
struct Nxt {
    GlyphCode nextGlyphCode : 15;
    bool stop : 1;
};
union ReplDisp {
    struct {
        GlyphCode replGlyphCode : 15;
        bool isAKern : 1;
    } repl;
    struct {
        FIX14 kerningValue : 14;
        bool isAGoTo : 1;
        bool isAKern : 1;
    } kern;
    struct {
        uint16_t displacement : 14;
        bool isAGoTo : 1;
        bool isAKern : 1;
    } goTo;
};

struct LigKernStep {
    Nxt a;
    ReplDisp b;
};
#endif

typedef LigKernStep (*LigKernStepsPtr)[];

struct RLEMetrics {
    uint8_t dynF : 4;      // Compression factor
    bool firstIsBlack : 1; // First pixels in compressed format are black
    uint8_t beforeAddedOptKern : 2;
    uint8_t afterAddedOptKern : 1;
};

struct GlyphInfo {
    uint8_t bitmapWidth;     // Width of bitmap once decompressed
    uint8_t bitmapHeight;    // Height of bitmap once decompressed
    int8_t horizontalOffset; // Horizontal offset from the orign
    int8_t verticalOffset;   // Vertical offset from the origin
    uint16_t packetLength;   // Length of the compressed bitmap
    FIX16 advance;           // Normal advance to the next glyph position in line
    RLEMetrics rleMetrics;   // RLE Compression information
    uint8_t ligKernPgmIndex; // = 255 if none, Index in the ligature/kern array
    GlyphCode mainCode;      // Main composite (or not) glyphCode for kerning matching algo
};
typedef GlyphInfo (*GlyphsInfoPtr)[];

// clang-format off
// 
// For FontFormat 1 (FontFormat::UTF32), there is a table that contains
// corresponding values between Unicode CodePoints and their internal GlyphCode.
// The GlyphCode is the index in the glyph table for the character.
//
// This table is in two parts:
//
// - Unicode plane information for the 4 planes supported
//   by the driver,
// - The list of bundle of code points that are part of each plane.
//   A bundle identifies the first codePoint and the number of consecutive codepoints
//   that are part of the bundle.
//
// For more information about the planes, please consult the followint Wikipedia page:
//
//     https://en.wikipedia.org/wiki/Plane_(Unicode)
//
// clang-format on

struct Plane {
    uint16_t codePointBundlesIdx; // Index of the plane in the CodePointBundles table
    uint16_t entriesCount;        // The number of entries in the CodePointBungles table
    GlyphCode firstGlyphCode;     // glyphCode corresponding to the first codePoint in the bundles
};

struct CodePointBundle {
    char16_t firstCodePoint; // The first UTF16 codePoint of the bundle
    char16_t lastCodePoint;  // The last UTF16 codePoint of the bundle
};

typedef Plane Planes[4];
typedef CodePointBundle (*CodePointBundlesPtr)[];
typedef Plane (*PlanesPtr)[];

#pragma pack(pop)

struct GlyphMetrics {
    int16_t xoff, yoff; // Used when the glyph is retrieved for caching
    int16_t descent;
    FIX16 advance;                   // Normal advance to the next glyph position in line
    int16_t lineHeight;              // This is the normal line height for all glyphs in the face
    int16_t ligatureAndKernPgmIndex; // Index of the ligature/kerning pgm for the glyph
    void clear() {
        xoff = yoff = 0;
        advance = lineHeight = 0;
        ligatureAndKernPgmIndex = 255;
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

// Used in context of both supported Font Formats.

const constexpr GlyphCode DONT_CARE_CODE = 0x7FFC;
const constexpr GlyphCode ZERO_WIDTH_CODE = 0x7FFD;
const constexpr GlyphCode SPACE_CODE = 0x7FFE;
const constexpr GlyphCode NO_GLYPH_CODE = 0x7FFF;

const constexpr char32_t ZERO_WIDTH_CODEPOINT = 0xFEFF; // U+0FEFF
const constexpr char32_t UNKNOWN_CODEPOINT = 0xE05E;    // U+E05E This is part of the Sol Font.

} // namespace ibmf_defs