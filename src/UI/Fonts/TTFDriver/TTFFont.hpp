#pragma once

#include "config.h"

#if TTF_SUPPORT

#include <cstdio>
#include <functional>

#include "TTFDefs.hpp"
#include "TTFFontData.hpp"
#include "UI/Fonts/Font.hpp"
#include "UI/Fonts/UTF8Iterator.hpp"

class Font {
private:
    static constexpr char const *TAG = "TTFFont";

    FT_Face face_;

    bool initialized_{false};
    FontData *fontData_;
    int faceIndex_;

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

        int error = FT_Load_Glyph(face_,              /* handle to face object */
                                  glyphCode,          /* glyph index           */
                                  FT_LOAD_NO_BITMAP); /* get only metrics      */

        if (error) {
            return false;
        }

        xoff = face_->glyph->bitmap_left;
        advance = face_->glyph->advance.x >> 6;
        glyphWidth = face_->glyph->bitmap.width;

        // if ((glyphCode == SPACE_CODE) ||
        //     ((*glyphsInfo_)[glyphCode].bitmapWidth == 0)) { // send as a space character
        //     *xoff = 0;
        //     *advance = static_cast<FIX16>(static_cast<uint16_t>(faceHeader_->spaceSize) << 6);
        //     return true;
        // }

        // if (glyphCode >= faceHeader_->glyphCount) {
        //     return false;
        // }

        // GlyphInfo *glyphInfo = &(*glyphsInfo_)[glyphCode];

        // if (glyphInfo == nullptr) {
        //     return false;
        // }

        // FIX16 adj = glyphInfo->advance < ((glyphInfo->bitmapWidth + 1) << 6) ? (1 << 6) : 0;

        // // NOLINTNEXTLINE(bugprone-signed-char-misuse, cert-str34-c)
        // *xoff = static_cast<int16_t>(glyphInfo->horizontalOffset);
        // *advance = glyphInfo->advance + adj;

        return true;
    }

    [[nodiscard]] inline auto getGlyphHOffset(GlyphCode glyphCode) -> int8_t {
        Glyph glyph;

        if (getGlyphMetrics(glyphCode, glyph)) {
            return glyph.metrics.yoff;
        }

        return 0;
    }

    auto ligKern(const GlyphCode glyphCode1, GlyphCode *glyphCode2, FIX16 *kern) const -> bool;
    auto getGlyph(GlyphCode glyphCode, Glyph &appGlyph, bool loadBitmap, bool caching = true,
                  Pos atPos = Pos(0, 0), bool inverted = false) -> bool;
    auto getGlyphMetrics(GlyphCode glyphCode, Glyph &appGlyph) -> bool;

    void copyBitmap(Bitmap &to, Bitmap &from, Pos atPos, bool inverted);

public:
    Font(FontData &fontData, int index) noexcept : fontData_(&fontData), faceIndex_(index) {
        initialized_ = false;

        if (fontData_->isInitialized()) {
            int error =
                FT_New_Memory_Face(fontData_->getLibrary(), (const FT_Byte *)fontData_->getData(),
                                   fontData_->getDataLength(), 0, &face_);
            if (error) {
                LOGE("The memory font format is unsupported or is broken (%d).", error);
            } else {
                int16_t size;
                switch (index) {
                case 0:
                    size = 8;
                    break;
                case 1:
                    size = 10;
                    break;
                case 2:
                    size = 12;
                    break;
                default:
                    size = 10;
                    break;
                }
                int error = FT_Set_Char_Size(face_,     // handle to face object
                                             0,         // char_width in 1/64th of points
                                             size * 64, // char_height in 1/64th of points
                                             150,       // horizontal device resolution
                                             150);      // vertical device resolution
                if (error) {
                    LOGE("Unable to set font size.");
                } else {
                    unknownGlyphCode_ = translate(UNKNOWN_CODEPOINT);
                    initialized_ = true;
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

    auto translate(char32_t codePoint) const -> GlyphCode;

    void drawSingleLineOfText(font_defs::Bitmap &canvas, font_defs::Pos pos,
                              const std::string &line, bool inverted);
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

        while (*buffer) {
            int16_t xoff;
            int16_t advance;
            int16_t glyphWidth;
            GlyphCode glyphCode = translate(toChar32(&buffer));

            if (getGlyphHorizontalMetrics(glyphCode, xoff, advance, glyphWidth)) {
                width += (*buffer == '\0') ? (glyphWidth - xoff) : advance;
            }
        }
        // log_w(" width: %" PRIi16, width);
        return width;
    }
};

#endif
