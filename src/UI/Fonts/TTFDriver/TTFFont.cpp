#include "config.h"

#if CONFIG_FONT_TTF

#include <algorithm>
#include <esp_heap_caps.h>
#include <execution>
#include <optional>

#include "TTFFont.hpp"

/**
 * @brief Translate UTF32 codePoint to it's internal representation
 *
 * Retrieves the
 * glyph code corresponding to the CodePoint.
 *
 * @param codePoint The UTF32 character code
 * @return The internal representation of CodePoint
 */
[[nodiscard]] auto Font::translate(char32_t codePoint) const -> GlyphCode {
    GlyphCode glyphCode = unknownGlyphCode_;

    if ((codePoint == ' ') || (codePoint == 0xA0) || (codePoint == 0x202F) ||
        ((codePoint >= 0x2000) && (codePoint <= 0x200F))) {
        glyphCode = SPACE_CODE;
    } else if ((codePoint >= 0xE000) && (codePoint <= 0xF8FF)) {
        // Those are codepoints in the private space. Their index starts at 0x8000.
        glyphCode = FT_Get_Char_Index(privateFace_, codePoint) + 0x8000;
        // LOGW("glyphCode for CodePoint U+%05" PRIx32 ": %" PRIu16, codePoint, glyphCode);
    } else {
        glyphCode = FT_Get_Char_Index(face_, codePoint);
    }

    if (glyphCode == 0) {
        glyphCode = unknownGlyphCode_;
    }

    // The following test could generates many entries in the log, depending on the quantity of
    // unknown code points received to be translated. Could be removed or disabled if required.
    if ((codePoint != UNKNOWN_CODEPOINT) && (glyphCode == unknownGlyphCode_)) {
        LOGW("Unknown Code Point received: U+%05" PRIx32, uint32_t(codePoint));
    }

    return glyphCode;
}

auto Font::ligKern(const GlyphCode glyphCode1, GlyphCode *glyphCode2, FIX16 *kern) const -> bool {

    // Is not checking for private font

    if ((glyphCode1 >= face_->num_glyphs) || (*glyphCode2 >= face_->num_glyphs)) {
        return false;
    }

    bool res = fontData_.ligKern(glyphCode1, glyphCode2, kern);

    if (*kern != 0) {
        *kern = FT_MulFix(*kern, face_->size->metrics.x_scale);
    }

    return res;
}

void Font::copyBitmap(Bitmap &to, const Bitmap &from, Pos atPos, bool inverted) {
    if (fontPixelResolution_ == font_defs::PixelResolution::ONE_BIT) {

        if (displayPixelResolution_ == PixelResolution::EIGHT_BITS) {
            uint8_t data;
            auto fromPtr = from.pixels;
            auto toPtr = to.pixels + static_cast<size_t>(atPos.y * to.pitch);

            for (uint16_t fromRow = 0; fromRow < from.dim.height;
                 fromRow++, toPtr += to.pitch, fromPtr += from.pitch) {
                int toIdx = atPos.x;
                int fromIdx = 0;
                uint8_t fromMask = 0;
                if (inverted) {
                    for (uint16_t i = 0; i < from.dim.width; i++) {
                        if (fromMask == 0) {
                            fromMask = 0x80;
                            data = fromPtr[fromIdx++];
                        }
                        if (data & fromMask) {
                            toPtr[toIdx] = 0xFF;
                        }
                        fromMask >>= 1;
                        toIdx += 1;
                    }
                } else {
                    for (uint16_t i = 0; i < from.dim.width; i++) {
                        if (fromMask == 0) {
                            fromMask = 0x80;
                            data = fromPtr[fromIdx++];
                        }
                        if (data & fromMask) {
                            toPtr[toIdx] = 0;
                        }
                        fromMask >>= 1;
                        toIdx += 1;
                    }
                }
            }
        } else {
            uint8_t data;
            auto fromPtr = from.pixels;
            auto toPtr = to.pixels + static_cast<size_t>(atPos.y * to.pitch);

            for (uint16_t fromRow = 0; fromRow < from.dim.height;
                 fromRow++, toPtr += to.pitch, fromPtr += from.pitch) {
                uint8_t toMask = 0x80 >> (atPos.x & 7);
                int toIdx = (atPos.x >> 3);
                int fromIdx = 0;
                uint8_t fromMask = 0;
                if (inverted) {
                    for (uint16_t i = 0; i < from.dim.width; i++) {
                        if (fromMask == 0) {
                            fromMask = 0x80;
                            data = fromPtr[fromIdx++];
                        }
                        if (data & fromMask) {
                            toPtr[toIdx] |= toMask;
                        }
                        fromMask >>= 1;
                        toMask >>= 1;
                        if (toMask == 0) {
                            toIdx += 1;
                            toMask = 0x80;
                        }
                    }
                } else {
                    for (uint16_t i = 0; i < from.dim.width; i++) {
                        if (fromMask == 0) {
                            fromMask = 0x80;
                            data = fromPtr[fromIdx++];
                        }
                        if (data & fromMask) {
                            toPtr[toIdx] &= ~toMask;
                        }
                        fromMask >>= 1;
                        toMask >>= 1;
                        if (toMask == 0) {
                            toIdx += 1;
                            toMask = 0x80;
                        }
                    }
                }
            }
        }
    } else { // Font Resolution EIGHT_BITS
        auto rowCount = from.dim.height;
        auto fromPtr = from.pixels;
        auto toPtr = &to.pixels[to.pitch * atPos.y + atPos.x];
        while (rowCount-- > 0) {
            if (inverted) {
                for (uint16_t i = 0; i < from.dim.width; i++) {
                    if (fromPtr[i] != 0) {
                        toPtr[i] = fromPtr[i];
                    }
                }
            } else {
                for (uint16_t i = 0; i < from.dim.width; i++) {
                    if (fromPtr[i] != 0) {
                        toPtr[i] = 255 - fromPtr[i];
                    }
                }
            }
            fromPtr += from.pitch;
            toPtr += to.pitch;
        }
    }
}

// Get a Glyph from the font to put in cache.
//
// Parameters:
//
//   glyphCode  : The index in the font to retrieve the glyph from
//   glyph      : The Glyph structure to put the information in

auto Font::getGlyphForCache(GlyphCode glyphCode, Glyph &glyph) -> bool {

    int error;

    FT_Face theFace = (glyphCode >= 0x8000) ? privateFace_ : face_;

    GlyphCode theGlyphCode = (glyphCode >= 0x8000) ? glyphCode - 0x8000 : glyphCode;

    glyph.clear();

    error = FT_Load_Glyph(theFace,          /* handle to face object */
                          theGlyphCode,     /* glyph index           */
                          FT_LOAD_DEFAULT); /* load flags */
    if (error) {
        LOGE("Unable to load glyph for charcode: %d", theGlyphCode);
        return false;
    }

    FT_GlyphSlot slot = theFace->glyph;

    if (theFace->glyph->format != FT_GLYPH_FORMAT_BITMAP) {
        if (fontPixelResolution_ == font_defs::PixelResolution::ONE_BIT) {
            error = FT_Render_Glyph(theFace->glyph,       // glyph slot
                                    FT_RENDER_MODE_MONO); // render mode
        } else {
            error = FT_Render_Glyph(theFace->glyph,         // glyph slot
                                    FT_RENDER_MODE_NORMAL); // render mode
        }

        if (error) {
            LOGE("Unable to render glyph for charcode: %d error: %d", theGlyphCode, error);
            return false;
        }
    }

    auto dim = Dim(slot->bitmap.width, slot->bitmap.rows);

    glyph.bitmap.dim = dim;
    glyph.bitmap.pitch = slot->bitmap.pitch;

    uint16_t size = dim.height * glyph.bitmap.pitch;

    if (size > 0) {
        glyph.bitmap.pixels =
            static_cast<font_defs::MemoryPtr>(heap_caps_malloc(size, MALLOC_CAP_SPIRAM));
        if (glyph.bitmap.pixels == nullptr) {
            LOGE("Unable to allocate glyph pixel memory of size %d!", size);
            glyph.bitmap.dim = Dim(0, 0);
            return false;
        }
        memcpy(glyph.bitmap.pixels, slot->bitmap.buffer, size);
    }

    glyph.metrics = {.xoff = static_cast<int16_t>(-slot->bitmap_left),
                     .yoff = static_cast<int16_t>(-slot->bitmap_top),
                     .descent = static_cast<int16_t>((slot->bitmap.rows + slot->bitmap_top) > 0
                                                         ? slot->bitmap.rows + slot->bitmap_top
                                                         : 0),
                     .advance = static_cast<FIX16>(slot->advance.x),
                     .lineHeight = static_cast<int16_t>(theFace->size->metrics.height >> 6)};
    return true;
}

auto Font::ligKernUTF8Map(const std::string &line, LigKernMappingHandler handler) const -> void {
    if (line.length() != 0) {
        auto iter = UTF8Iterator(line);
        auto glyphCode1 = translate(*iter++);
        auto glyphCode2 = (iter == line.end()) ? NO_GLYPH_CODE : translate(*iter++);
        font_defs::FIX16 kern;
        bool firstWordChar = true;
        bool wasEndOfWord = false;
        while (glyphCode1 != NO_GLYPH_CODE) {
            if (wasEndOfWord && (glyphCode1 != SPACE_CODE)) {
                wasEndOfWord = false;
                firstWordChar = true;
            }
            kern = static_cast<FIX16>(0);

            // Ligature loop for glyphCode1
            while (ligKern(glyphCode1, &glyphCode2, &kern)) {
                glyphCode1 = glyphCode2;
                glyphCode2 = (iter == line.end()) ? NO_GLYPH_CODE : translate(*iter++);
            }

            // Ligature loop for glyphCode2
            auto glyphCode3 = (iter == line.end()) ? NO_GLYPH_CODE : translate(*iter);
            if (glyphCode3 != NO_GLYPH_CODE) {
                bool someLig = false;
                FIX16 k;
                while (ligKern(glyphCode2, &glyphCode3, &k)) {
                    glyphCode2 = glyphCode3;
                    glyphCode3 = (iter == line.end()) ? NO_GLYPH_CODE : translate(*++iter);
                    someLig = true;
                }
                if (someLig) {
                    ligKern(glyphCode1, &glyphCode2, &kern);
                }
            }

            bool lastWordChar = (glyphCode2 == SPACE_CODE) || (glyphCode2 == NO_GLYPH_CODE);

            (handler)(glyphCode1, kern, firstWordChar, lastWordChar);

            firstWordChar = false;
            if (lastWordChar) {
                wasEndOfWord = true;
            }
            glyphCode1 = glyphCode2;
            glyphCode2 = (iter == line.end()) ? NO_GLYPH_CODE : translate(*iter++);
        }
    }
}

// Returns the x position at the end of string
auto Font::drawSingleLineOfText(font_defs::Bitmap &canvas, font_defs::Pos pos,
                                const std::string &line, bool inverted) -> int {
    font_defs::Pos atPos = pos;

    if constexpr (TTF_TRACING) {
        LOGD("drawSingleLineOfText()");
    }
    if (isInitialized()) {
        // Glyph glyph{};

        // We set here the canvas pitch value, as there is still some definitions required at
        // the application-level in regard of the upcoming Sol Glasse augmented resolution.
        //
        // The following may require some modification as the next Sol Glasses version
        // may be using a different pitch than the one computed here.

        atPos.y += lineHeight() + (face_->size->metrics.descender >> 6);

        canvas.pitch = (displayPixelResolution_ == PixelResolution::ONE_BIT)
                           ? (canvas.dim.width + 7) >> 3
                           : canvas.dim.width;

        ligKernUTF8Map(line, [this, &canvas, &atPos, inverted](GlyphCode glyphCode, FIX16 kern,
                                                               bool first, bool last) {
            if (glyphCode == SPACE_CODE) {
                atPos.x += (spaceSize_ >> 6);
            } else {
                std::optional<const Glyph *> glyph = fontData_.cache.getGlyph(
                    *this, glyphCode, subSupSize_ >= 0 ? subSupSize_ : size_);

                if (glyph.has_value()) {
                    if (first) {
                        atPos.x += glyph.value()->metrics.xoff;
                    }

                    if (glyph.value()->bitmap.dim.width > 0) {
                        lastGlyphWidth_ = glyph.value()->bitmap.dim.width;
                        // TODO: Ask Guy about the right way to handle line height and keeping the
                        // full text inside its box.
                        Pos outPos = Pos(atPos.x - glyph.value()->metrics.xoff,
                                         atPos.y + glyph.value()->metrics.yoff);
                        copyBitmap(canvas, glyph.value()->bitmap, outPos, inverted);
                    }

                    // As advance is positive and greather than kern, we can shift right
                    // to get rid of the fix point decimals
                    atPos.x += last ? lastGlyphWidth_ - (kern / 64) - glyph.value()->metrics.xoff
                                    : ((glyph.value()->metrics.advance + kern) >> 6);
                }
            }
        });
    }

    return atPos.x; // In case the text is just a segment of the line
}

auto Font::getTextSize(const std::string &buffer) -> font_defs::Dim {
    font_defs::Dim dim = font_defs::Dim(0, 0);
    int16_t up = 0;
    int16_t down = 0;

    if (isInitialized()) {
        ligKernUTF8Map(buffer, [this, &dim, &up, &down](GlyphCode glyphCode, FIX16 kern, bool first,
                                                        bool last) {
            if (glyphCode == SPACE_CODE) {
                dim.width += spaceSize_ >> 6;
            } else {
                std::optional<const Glyph *> glyph = fontData_.cache.getGlyph(
                    *this, glyphCode, subSupSize_ >= 0 ? subSupSize_ : size_);
                if (glyph.has_value()) {
                    dim.width += last ? glyph.value()->bitmap.dim.width - (kern / 64) -
                                            glyph.value()->metrics.xoff
                                      : ((glyph.value()->metrics.advance + kern) >> 6);

                    up = (up < glyph.value()->metrics.yoff) ? glyph.value()->metrics.yoff : up;
                    down = (down < glyph.value()->metrics.descent) ? glyph.value()->metrics.descent
                                                                   : down;
                }
            }
        });
    }

    dim.height = (up + down);
    // LOGD("TTFFont: TextBound: width: %d, height: %d", dim.width, dim.height);
    return dim;
}

auto Font::getTextWidth(const std::string &buffer) -> int {
    int width = 0;
    if (isInitialized()) {
        ligKernUTF8Map(buffer,
                       [this, &width](GlyphCode glyphCode, FIX16 kern, bool first, bool last) {
            if (glyphCode == SPACE_CODE) {
                width += spaceSize_ >> 6;
            } else {
                std::optional<const Glyph *> glyph = fontData_.cache.getGlyph(
                    *this, glyphCode, subSupSize_ >= 0 ? subSupSize_ : size_);

                if (glyph.has_value()) {
                    width += last ? glyph.value()->bitmap.dim.width - (kern / 64) -
                                        glyph.value()->metrics.xoff
                                  : ((glyph.value()->metrics.advance + kern) >> 6);
                }
            }
        });
    }

    return width;
}

auto Font::toChar32(const char **str) -> char32_t {
    const auto *s = reinterpret_cast<const uint8_t *>(*str);
    uint8_t c1 = *s++;

    char32_t res;

    if (c1 >= 0xC0) {
        uint8_t c2 = *s++;

        if (c1 >= 0xE0) {
            uint8_t c3 = *s++;

            if (c1 >= 0xF0) {
                uint8_t c4 = *s++;

                res = ((c1 & 0b00000111) << 18) | ((c2 & 0b00111111) << 12) |
                      ((c3 & 0b00111111) << 6) | (c4 & 0b00111111);
            } else {
                res = ((c1 & 0b00001111) << 12) | ((c2 & 0b00111111) << 6) | (c3 & 0b00111111);
            }
        } else {
            res = ((c1 & 0b00011111) << 6) | (c2 & 0b00111111);
        }

    } else {
        res = c1 & 0b01111111;
    }

    *str = reinterpret_cast<const char *>(s);
    return res;
}

auto Font::getTextHeight(const std::string &buffer) -> int {

    int height = 0;
    if (isInitialized()) {
        Dim dim = getTextSize(buffer);
        height = dim.height;
    }

    return height;
}

#endif