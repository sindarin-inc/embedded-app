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
    FT_Face face_{};
    FT_Face privateFace_{};

    bool initialized_{false};
    FontData &fontData_;
    int size_;
    int subSupSize_{-1};
    FIX16 spaceSize_{0};
    uint8_t lastGlyphWidth_{};
    PixelResolution displayPixelResolution_{DEFAULT_DISPLAY_PIXEL_RESOLUTION};
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

    auto ligKern(const GlyphCode glyphCode1, GlyphCode *glyphCode2, FIX16 *kern) const -> bool;

    void copyBitmap(Bitmap &to, const Bitmap &from, Pos atPos, bool inverted);

public:
    Font(FontData &fontData, int size) noexcept : fontData_(fontData), size_(size) {

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

                    std::optional<const Glyph *> glyph = fontData_.cache.getGlyph(
                        *this, glyphCode, subSupSize_ >= 0 ? subSupSize_ : size_);

                    if (!glyph.has_value()) {
                        LOGE("Unable to load glyph for space char.");
                        spaceSize_ = 5 * 64; // Use a default size
                    } else {
                        spaceSize_ = glyph.value()->metrics.advance;
                    }

                    error = FT_New_Memory_Face(fontData_.getLibrary(),
                                               (const FT_Byte *)(fontData_.getPrivateData()),
                                               fontData_.getPrivateDataSize(), 0, &privateFace_);
                    if (error) {
                        LOGE("The memory of the private font format is unsupported or is "
                             "broken (%d).",
                             error);
                    } else {
                        error =
                            FT_Set_Char_Size(privateFace_,        // handle to face object
                                             0,                   // char_width in 1/64th of points
                                             size_ * 64,          // char_height in 1/64th of points
                                             SCREEN_RES_PER_INCH, // horizontal device resolution
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

    [[nodiscard]] inline auto getFontData() const -> FontData * { return &fontData_; }

    auto getGlyphForCache(GlyphCode glyphCode, Glyph &glyph) -> bool;

    [[nodiscard]] inline auto setDisplayPixelResolution(PixelResolution res) -> bool {
#if CONFIG_DISPLAY_PIXEL_RESOLUTION_IS_FIX
        log_w("The display does not allow to change it's pixel resolution!");
        return false;
#else
        if (initialized_) {
            if (displayPixelResolution_ != res) {
                // check for coherence of fontPixelResolution
                if ((res == PixelResolution::ONE_BIT) &&
                    (fontPixelResolution_ == PixelResolution::EIGHT_BITS)) {
                    setFontPixelResolution(PixelResolution::ONE_BIT);
                }
                displayPixelResolution_ = res;
            }
        }
        return true;
#endif
    }

    [[nodiscard]] inline auto getDisplayPixelResolution() const -> PixelResolution {
        return (initialized_) ? displayPixelResolution_ : DEFAULT_DISPLAY_PIXEL_RESOLUTION;
    }

    inline void setFontPixelResolution(PixelResolution res) {
        if (initialized_) {
            if (fontPixelResolution_ != res) {
                if ((res == PixelResolution::EIGHT_BITS) &&
                    (displayPixelResolution_ != PixelResolution::EIGHT_BITS)) {
                    log_e(
                        "Cannot set font resolution to EIGHT_BITS if the display resolution is not "
                        "EIGHT_BITS!");
                } else {
                    fontData_.cache.clear();
                    fontPixelResolution_ = res;
                }
            }
        }
    }

    [[nodiscard]] inline auto getFontPixelResolution() const -> PixelResolution {
        return (initialized_) ? fontPixelResolution_ : DEFAULT_FONT_PIXEL_RESOLUTION;
    }

    [[nodiscard]] auto translate(char32_t codePoint) const -> GlyphCode;

    auto drawSingleLineOfText(font_defs::Bitmap &canvas, font_defs::Pos pos,
                              const std::string &line, bool inverted) -> int;
    auto getTextSize(const std::string &buffer) -> font_defs::Dim;
    auto getTextWidth(const std::string &buffer) -> int;

    auto getTextHeight(const std::string &buffer) -> int;

    // Non-validating algorithm
    auto toChar32(const char **str) -> char32_t;

    // inline auto getTextWidthQuick(const char *buffer) -> int { return getTextWidth(buffer); }
    inline auto getTextWidthQuick(const char *buffer) -> int16_t {

        int16_t width = 0;

        while (*buffer) {

            GlyphCode glyphCode = translate(toChar32(&buffer));

            if (glyphCode == SPACE_CODE) {
                width += spaceSize_ >> 6;
            } else {
                std::optional<const Glyph *> glyph = fontData_.cache.getGlyph(
                    *this, glyphCode, subSupSize_ >= 0 ? subSupSize_ : size_);
                if (glyph.has_value()) {
                    width += (*buffer == '\0')
                                 ? (glyph.value()->bitmap.dim.width - glyph.value()->metrics.xoff)
                                 : (glyph.value()->metrics.advance >> 6);
                }
            }
        }

        return width;
    }

    inline void setSupSubFontSize() {
        subSupSize_ = (size_ - SUP_SUB_FONT_DOWNSIZING) * 64;
        int error = FT_Set_Char_Size(face_,                // handle to face object
                                     0,                    // char_width in 1/64th of points
                                     subSupSize_,          // char_height in 1/64th of points
                                     SCREEN_RES_PER_INCH,  // horizontal device resolution
                                     SCREEN_RES_PER_INCH); // vertical device resolution
        if (error) {
            LOGE("Unable to set font size.");
        }
    }

    inline void setNormalFontSize() {
        subSupSize_ = -1;
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
