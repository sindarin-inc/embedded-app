#pragma once

#include "config.h"

#if TTF_SUPPORT

#include <cstring>
#include <fstream>
#include <iostream>

#include "TTFDefs.hpp"

using namespace ttf_defs;

#include <ft2build.h>
#include FT_FREETYPE_H

/**
 * @brief Access to an TTF font.
 *
 * This is a low-level class to allow acces to an TTF font
 *
 */
class FontData {

private:
    static constexpr char const *TAG = "TTFFontData";

    static FT_Library library;

    bool initialized_{false};

    MemoryPtr fontData_{nullptr};
    uint32_t fontLength_{0};

    GlyphCode unknownGlyphCode_{0};

public:
    FontData(const uint8_t *fontData, uint32_t length) noexcept
        : fontData_(fontData), fontLength_(length) {

        load();
        if (!initialized_) {
            log_e("Font data not recognized!");
        }
    }

    FontData() = default;
    ~FontData() = default;

    [[nodiscard]] inline auto getFontFormat() const -> FontFormat {
        return (isInitialized()) ? preamble_->bits.fontFormat : FontFormat::UNKNOWN;
    }
    [[nodiscard]] inline auto isInitialized() const -> bool { return initialized_; }

    auto load() -> bool;

    [[nodiscard]] auto translate(char32_t codePoint) const -> GlyphCode;
};

#endif