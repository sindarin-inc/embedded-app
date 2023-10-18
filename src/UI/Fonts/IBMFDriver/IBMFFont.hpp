#pragma once

#include <cstdio>
#include <functional>

#include "IBMFFontLow.hpp"
#include "UI/Fonts/Font.hpp"
#include "UTF8Iterator.hpp"

class IBMFFont : public Font {
private:
    static constexpr char const *TAG = "IBMFFont";

    IBMFFontLow *font_;
    int faceIndex_;
    // IBMFFaceLow *face_;

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
    IBMFFont(IBMFFontLow &ibmFont, int index) noexcept
        : Font(FontType::IBMF), font_(&ibmFont), faceIndex_(index) {}

    [[nodiscard]] inline auto isInitialized() const -> bool {
        if ((font_ != nullptr) && font_->isInitialized() &&
            font_->getFace(faceIndex_)->isInitialized()) {
            return true;
        }

        LOGE("!!!! FONT NOT INITIALIZED !!!!");
        return false;
    }

    [[nodiscard]] inline auto getFace() -> const IBMFFaceLow * {
        return isInitialized() ? font_->getFace(faceIndex_) : nullptr;
    }

    inline void setResolution(PixelResolution res) {
        if constexpr (IBMF_TRACING) {
            LOGD("setResolution()");
        }
        if (isInitialized()) {
            font_->getFace(faceIndex_)->setResolution(res);
        }
    }

    [[nodiscard]] inline auto getResolution() const -> PixelResolution {
        if constexpr (IBMF_TRACING) {
            LOGD("getResolution()");
        }
        return (isInitialized()) ? font_->getFace(faceIndex_)->getResolution() : DEFAULT_RESOLUTION;
    }

    [[nodiscard]] inline auto lineHeight() const -> int override {
        if constexpr (IBMF_TRACING) {
            LOGD("lineHeight()");
            if (!isInitialized()) {
                LOGE("Not initialized!!!");
            }
        }
        return isInitialized() ? static_cast<int>(font_->getFace(faceIndex_)->getLineHeight()) : 0;
    }

    void drawSingleLineOfText(ibmf_defs::Bitmap &canvas, ibmf_defs::Pos pos,
                              const std::string &line, bool inverted) const;
    auto getTextSize(const std::string &buffer) -> ibmf_defs::Dim;
    auto getTextWidth(const std::string &buffer) -> int;

    auto getTextHeight(const std::string &buffer) -> int;

    // Non-validating algorithm
    auto toChar32(const char **str) -> char32_t;

    // inline auto getTextWidthQuick(const char *buffer) -> int { return getTextWidth(buffer); }
    inline auto getTextWidthQuick(const char *buffer) -> int16_t {
        if constexpr (IBMF_TRACING) {
            LOGD("getTextWidthQuick()");
        }

        int16_t width = 0;
        auto face = font_->getFace(faceIndex_);

        // log_w("word: %s", buffer);

        while (*buffer) {
            int16_t xoff;
            FIX16 advance;
            GlyphCode glyphCode = font_->translate(toChar32(&buffer));

            if (face->getGlyphHorizontalMetrics(glyphCode, &xoff, &advance)) {
                width +=
                    (*buffer == '\0') ? (face->getGlyphWidth(glyphCode) - xoff) : (advance >> 6);
            }
        }
        // log_w(" width: %" PRIi16, width);
        return width;
    }
};
