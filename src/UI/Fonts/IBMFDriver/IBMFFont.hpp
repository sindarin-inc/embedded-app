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

    inline auto setDisplayPixelResolution(PixelResolution res) -> bool {
#if CONFIG_DISPLAY_PIXEL_RESOLUTION_IS_FIX
        log_w("The display does not allow to change it's pixel resolution!");
        return false;
#else

        if constexpr (IBMF_TRACING) {
            LOGD("setDisplayPixelResolution_()");
        }
        if (isInitialized()) {
            fontData_->getFace(faceIndex_)->setDisplayPixelResolution(res);
            return true;
        }
#endif
    }

    [[nodiscard]] inline auto getDisplayPixelResolution() const -> PixelResolution {
        if constexpr (IBMF_TRACING) {
            LOGD("getDisplayPixelResolution_()");
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

    auto drawSingleLineOfText(ibmf_defs::Bitmap &canvas, ibmf_defs::Pos pos,
                              const std::string &line, bool inverted) const -> int;
    [[nodiscard]] auto getTextSize(const std::string &buffer) const -> ibmf_defs::Dim;
    auto getTextWidth(const std::string &buffer) -> int;

    [[nodiscard]] auto getTextHeight(const std::string &buffer) const -> int;

    // Non-validating algorithm
    auto toChar32(const char **str) -> char32_t;

    // inline auto getTextWidthQuick(const char *buffer) -> int { return getTextWidth(buffer); }
    inline auto getTextWidthQuick(const char *buffer) -> int16_t {
        if constexpr (IBMF_TRACING) {
            LOGD("getTextWidthQuick()");
        }

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
        // log_w(" width: %" PRIi16, width);
        return width;
    }
};

#endif