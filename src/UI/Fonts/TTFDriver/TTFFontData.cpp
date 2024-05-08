#include "config.h"

#if TTF_SUPPORT

#include "TTFFontData.hpp"

FT_Library FontData::library = nullptr;

auto FontData::load() -> bool {

    initialized_ = false;

    if (library == nullptr) {
        int error = FT_Init_FreeType(&library);
        if (error) {
            log_e("An error occurred during FreeType library initialization.");
        }
    }

    set_font_face_from_memory(buffer, buffer_size);

    initialized_ = true;
    unknownGlyphCode_ = translate(UNKNOWN_CODEPOINT);

    return true;
}

/**
 * @brief Translate UTF32 codePoint to it's internal representation
 *
 * Retrieves the
 * glyph code corresponding to the CodePoint.
 *
 * @param codePoint The UTF32 character code
 * @return The internal representation of CodePoint
 */
[[nodiscard]] auto FontData::translate(char32_t codePoint) const -> GlyphCode {
    GlyphCode glyphCode = unknownGlyphCode_;

    if ((codePoint == ' ') || (codePoint == 0xA0) || (codePoint == 0x202F) ||
        ((codePoint >= 0x2000) && (codePoint <= 0x200F))) {
        glyphCode = SPACE_CODE;
    } else if (codePoint == ZERO_WIDTH_CODEPOINT) {
        glyphCode = ZERO_WIDTH_CODE;
    } else if ((codePoint >= 0xFFF0) && (codePoint <= 0xFFFF)) {
        glyphCode = DONT_CARE_CODE;
    } else {
    }

    // The following test could generates many entries in the log, depending on the quantity of
    // unknown code points received to be translated. Could be removed or disabled if required.
    if ((codePoint != UNKNOWN_CODEPOINT) && (glyphCode == unknownGlyphCode_)) {
        log_w("Unknown Code Point received: U+%05" PRIx32, uint32_t(codePoint));
    }

    return glyphCode;
}

#endif