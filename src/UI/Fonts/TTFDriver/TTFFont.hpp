#pragma once

#include "config.h"

#if CONFIG_FONT_TTF

#include <cstdio>
#include <functional>

#include "TTFDefs.hpp"
#include "TTFFontData.hpp"
#include "UI/Fonts/Font.hpp"
#include "UI/Fonts/UTF8Iterator.hpp"

class Font {
private:
    FT_Face face_;
    FT_Face privateFace_;

    bool initialized_{false};
    FontData &fontData_;
    int size_;
    FIX16 spaceSize_{0};
    uint8_t lastGlyphWidth_;
    PixelResolution pixelResolution_{DEFAULT_PIXEL_RESOLUTION};
    PixelResolution fontPixelResolution_{DEFAULT_FONT_PIXEL_RESOLUTION};

    GlyphCode unknownGlyphCode_{0};

    // Maximum size of an allocated buffer to do vsnprintf formatting
    static constexpr int MAX_SIZE = 100;

    typedef std::function<void(GlyphCode, FIX16, bool, bool)> const &LigKernMappingHandler;

    /// @brief Ligature/Kerning/UTF8 Mapper
    ///
    /// Iterates on each UTF8 character present in **line**, sending to the
    /// **handler** the corresponding GlyphCode and kerning values after
    /// applying the LigKern program to the character.
    ///
    /// @param line In. The UTF8 compliant string of character.
    /// @param handler Call. The callback closure handler.
    ///
    auto ligKernUTF8Map(const std::string &line, LigKernMappingHandler handler) const -> void;

    inline auto getGlyphHorizontalMetrics(GlyphCode glyphCode, int16_t &xoff, int16_t &advance,
                                          int16_t &glyphWidth) -> bool {

        FT_Face theFace = (glyphCode >= 0x8000) ? privateFace_ : face_;
        GlyphCode theGlyphCode = (glyphCode >= 0x8000) ? glyphCode - 0x8000 : glyphCode;

        int error = FT_Load_Glyph(theFace,            /* handle to face object */
                                  theGlyphCode,       /* glyph index           */
                                  FT_LOAD_NO_BITMAP); /* get only metrics      */

        if (error) {
            return false;
        }

        xoff = theFace->glyph->bitmap_left;
        advance = theFace->glyph->advance.x >> 6;
        glyphWidth = theFace->glyph->bitmap.width;

        return true;
    }

    [[nodiscard]] inline auto getGlyphHOffset(GlyphCode glyphCode) -> int8_t {
        Glyph glyph;

        if (getGlyphMetrics(glyphCode, glyph)) {
            return glyph.metrics.xoff;
        }

        return 0;
    }

    auto ligKern(const GlyphCode glyphCode1, GlyphCode *glyphCode2, FIX16 *kern) const -> bool;
    auto getGlyph(GlyphCode glyphCode, Glyph &appGlyph, bool loadBitmap, bool caching = true,
                  Pos atPos = Pos(0, 0), bool inverted = false) -> bool;
    auto getGlyphMetrics(GlyphCode glyphCode, Glyph &appGlyph) -> bool;

    void copyBitmap(Bitmap &to, bool toHeightBit, Bitmap &from, Pos atPos, bool inverted);

public:
    Font(FontData &fontData, int size) noexcept : fontData_(fontData), size_(size) {
        initialized_ = false;

        if (fontData_.isInitialized()) {
            int error =
                FT_New_Memory_Face(fontData_.getLibrary(), (const FT_Byte *)(fontData_.getData()),
                                   fontData_.getDataSize(), 0, &face_);
            if (error) {
                LOGE("The memory of the main font format is unsupported or is broken (%d).", error);
            } else {
                int error = FT_Set_Char_Size(face_,      // handle to face object
                                             0,          // char_width in 1/64th of points
                                             size_ * 64, // char_height in 1/64th of points
                                             150,        // horizontal device resolution
                                             150);       // vertical device resolution
                if (error) {
                    LOGE("Unable to set font size.");
                } else {

                    GlyphCode glyphCode = FT_Get_Char_Index(face_, ' ');

                    error = FT_Load_Glyph(face_,              /* handle to face object */
                                          glyphCode,          /* glyph index           */
                                          FT_LOAD_NO_BITMAP); /* load flags */
                    if (error) {
                        LOGE("Unable to load glyph for space char.");
                    } else {
                        spaceSize_ = static_cast<FIX16>(face_->glyph->advance.x);
                        error = FT_New_Memory_Face(
                            fontData_.getLibrary(), (const FT_Byte *)(fontData_.getPrivateData()),
                            fontData_.getPrivateDataSize(), 0, &privateFace_);
                        if (error) {
                            LOGE("The memory of the private font format is unsupported or is "
                                 "broken (%d).",
                                 error);
                        } else {
                            error = FT_Set_Char_Size(
                                privateFace_,         // handle to face object
                                0,                    // char_width in 1/64th of points
                                size_ * 64,           // char_height in 1/64th of points
                                SCREEN_RES_PER_INCH,  // horizontal device resolution
                                SCREEN_RES_PER_INCH); // vertical device resolution
                            if (error) {
                                LOGE("Unable to set private font size.");
                            }

                            unknownGlyphCode_ = translate(UNKNOWN_CODEPOINT);
                            initialized_ = true;
                        }
                    }
                }
            }
        }
    }
    ~Font() = default;

    [[nodiscard]] inline auto isInitialized() const -> bool {
        if (initialized_) {
            return true;
        }

        LOGE("!!!! FONT NOT INITIALIZED !!!!");
        return false;
    }

    [[nodiscard]] inline auto lineHeight() const -> int {
        if constexpr (TTF_TRACING) {
            LOGD("lineHeight()");
        }
        return isInitialized() ? face_->size->metrics.height >> 6 : 0;
    }

    inline void setPixelResolution(PixelResolution res) { pixelResolution_ = res; }

    [[nodiscard]] inline auto getPixelResolution() const -> PixelResolution {
        return (initialized_) ? pixelResolution_ : DEFAULT_PIXEL_RESOLUTION;
    }

    inline void setFontPixelResolution(PixelResolution res) { fontPixelResolution_ = res; }

    [[nodiscard]] inline auto getFontPixelResolution() const -> PixelResolution {
        return (initialized_) ? fontPixelResolution_ : DEFAULT_FONT_PIXEL_RESOLUTION;
    }

    auto translate(char32_t codePoint) const -> GlyphCode;

    auto drawSingleLineOfText(font_defs::Bitmap &canvas, font_defs::Pos pos,
                              const std::string &line, bool inverted) -> int;
    auto getTextSize(const std::string &buffer) -> font_defs::Dim;
    auto getTextWidth(const std::string &buffer) -> int;

    auto getTextHeight(const std::string &buffer) -> int;

    // Non-validating algorithm
    auto toChar32(const char **str) -> char32_t;

    // inline auto getTextWidthQuick(const char *buffer) -> int { return getTextWidth(buffer); }
    inline auto getTextWidthQuick(const char *buffer) -> int16_t {
        if constexpr (TTF_TRACING) {
            LOGD("getTextWidthQuick()");
        }

        int16_t width = 0;

        // log_w("word: %s", buffer);

        const char *b = buffer;

        while (*buffer) {
            int16_t xoff;
            int16_t advance;
            int16_t glyphWidth;
            GlyphCode glyphCode = translate(toChar32(&buffer));

            if (getGlyphHorizontalMetrics(glyphCode, xoff, advance, glyphWidth)) {
                width += (*buffer == '\0') ? (glyphWidth - xoff) : advance;
            }
        }

        // log_w(" width quick: %" PRIi16 ": %s", width, b);
        return width;
    }

    inline void setSupSubFontSize() {
        int newSize = (size_ - SUP_SUB_FONT_DOWNSIZING) * 64;
        int error = FT_Set_Char_Size(face_,                // handle to face object
                                     0,                    // char_width in 1/64th of points
                                     newSize,              // char_height in 1/64th of points
                                     SCREEN_RES_PER_INCH,  // horizontal device resolution
                                     SCREEN_RES_PER_INCH); // vertical device resolution
        if (error) {
            LOGE("Unable to set font size.");
        }
    }

    inline void setNormalFontSize() {
        int error = FT_Set_Char_Size(face_,                // handle to face object
                                     0,                    // char_width in 1/64th of points
                                     size_ * 64,           // char_height in 1/64th of points
                                     SCREEN_RES_PER_INCH,  // horizontal device resolution
                                     SCREEN_RES_PER_INCH); // vertical device resolution
        if (error) {
            LOGE("Unable to set font size.");
        }
    }
};

#endif
