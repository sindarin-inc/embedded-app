#pragma once

#include "config.h"

#if CONFIG_FONT_IBMF

#include <cstdio>
#include <functional>

#include "IBMFFontData.hpp"
#include "UI/Fonts/Font.hpp"
#include "UI/Fonts/UTF8Iterator.hpp"

class Font {
private:
    FontData *fontData_;
    int faceIndex_;
    // IBMFFace *face_;
    uint8_t lastGlyphWidth_{};

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

    void copyBitmap(font_defs::Bitmap &to, const font_defs::Bitmap &from, font_defs::Pos atPos,
                    bool inverted);

public:
    Font(FontData &ibmfFont, int index) noexcept : fontData_(&ibmfFont), faceIndex_(index) {}

    ~Font() = default;

    [[nodiscard]] inline auto isInitialized() const -> bool {
        if ((fontData_ != nullptr) && fontData_->isInitialized() &&
            fontData_->getFace(faceIndex_)->isInitialized()) {
            return true;
        }

        LOGE("!!!! FONT NOT INITIALIZED !!!!");
        return false;
    }

    [[nodiscard]] inline auto getFace() -> const IBMFFace * {
        return isInitialized() ? fontData_->getFace(faceIndex_) : nullptr;
    }

    inline void setDisplayPixelResolution(PixelResolution res) {
        if constexpr (IBMF_TRACING) {
            LOGD("setDisplayPixelResolution()");
        }
        if (isInitialized()) {
            fontData_->getFace(faceIndex_)->setDisplayPixelResolution(res);
        }
    }

    [[nodiscard]] inline auto getDisplayPixelResolution() const -> PixelResolution {
        if constexpr (IBMF_TRACING) {
            LOGD("getDisplayPixelResolution()");
        }
        return (isInitialized()) ? fontData_->getFace(faceIndex_)->getDisplayPixelResolution()
                                 : DEFAULT_DISPLAY_PIXEL_RESOLUTION;
    }

    [[nodiscard]] inline auto lineHeight() const -> int {
        if constexpr (IBMF_TRACING) {
            LOGD("lineHeight()");
            if (!isInitialized()) {
                LOGE("Not initialized!!!");
            }
        }
        return isInitialized() ? static_cast<int>(fontData_->getFace(faceIndex_)->getLineHeight())
                               : 0;
    }

    inline auto getFontData() const -> FontData * { return fontData_; }

    auto drawSingleLineOfText(ibmf_defs::Bitmap &canvas, ibmf_defs::Pos pos,
                              const std::string &line, bool inverted) -> int;
    auto getTextSize(const std::string &buffer) -> ibmf_defs::Dim;
    auto getTextWidth(const std::string &buffer) -> int;

    auto getTextHeight(const std::string &buffer) -> int;

    // Non-validating algorithm
    auto toChar32(const char **str) -> char32_t;

    // inline auto getTextWidthQuick(const char *buffer) -> int { return getTextWidth(buffer); }
    inline auto getTextWidthQuick(const char *buffer) -> int16_t {

#if 0
        int16_t width = 0;
        auto face = fontData_->getFace(faceIndex_);

        // log_w("word: %s", buffer);

        while (*buffer) {
            int16_t xoff;
            FIX16 advance;
            GlyphCode glyphCode = fontData_->translate(toChar32(&buffer));

            if (face->getGlyphHorizontalMetrics(glyphCode, &xoff, &advance)) {
                width +=
                    (*buffer == '\0') ? (face->getGlyphWidth(glyphCode) - xoff) : (advance >> 6);
            }
        }
#else
        int16_t width = 0;
        auto face = fontData_->getFace(faceIndex_);

        while (*buffer) {

            GlyphCode glyphCode = fontData_->translate(toChar32(&buffer));

            if (glyphCode == SPACE_CODE) {
                width += face->getSpaceSize();
            } else {
                std::optional<const Glyph *> glyph =
                    fontData_->glyphCache_.getGlyph(*face, glyphCode, faceIndex_);
                if (glyph.has_value()) {
                    width += (*buffer == '\0')
                                 ? (glyph.value()->bitmap.dim.width - glyph.value()->metrics.xoff)
                                 : (glyph.value()->metrics.advance >> 6);
                }
            }
        }

#endif
        return width;
    }
};

#endif