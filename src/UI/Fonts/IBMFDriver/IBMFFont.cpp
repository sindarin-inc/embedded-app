#include "config.h"

#if CONFIG_FONT_IBMF

#include "IBMFFont.hpp"

void Font::copyBitmap(font_defs::Bitmap &to, const font_defs::Bitmap &from, font_defs::Pos atPos,
                      bool inverted) {

    if (getDisplayPixelResolution() == PixelResolution::EIGHT_BITS) {
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
                    if ((data & fromMask) == 0) {
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
                    if ((data & fromMask) == 0) {
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
                    if ((data & fromMask) == 0) {
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
                    if ((data & fromMask) == 0) {
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
}

auto Font::ligKernUTF8Map(const std::string &line, LigKernMappingHandler handler) const -> void {
    if (line.length() != 0) {
        auto iter = UTF8Iterator(line);
        auto glyphCode1 = fontData_->translate(*iter++);
        auto glyphCode2 = (iter == line.end()) ? NO_GLYPH_CODE : fontData_->translate(*iter++);
        FIX16 kern;
        bool firstWordChar = true;
        bool wasEndOfWord = false;
        while (glyphCode1 != NO_GLYPH_CODE) {
            if (wasEndOfWord && (glyphCode1 != SPACE_CODE)) {
                wasEndOfWord = false;
                firstWordChar = true;
            }
            kern = static_cast<FIX16>(0);

            // Ligature loop for glyphCode1
            while (fontData_->getFace(faceIndex_)->ligKern(glyphCode1, &glyphCode2, &kern)) {
                glyphCode1 = glyphCode2;
                glyphCode2 = (iter == line.end()) ? NO_GLYPH_CODE : fontData_->translate(*iter++);
            }

            // Ligature loop for glyphCode2
            auto glyphCode3 = (iter == line.end()) ? NO_GLYPH_CODE : fontData_->translate(*iter);
            if (glyphCode3 != NO_GLYPH_CODE) {
                bool someLig = false;
                FIX16 k;
                while (fontData_->getFace(faceIndex_)->ligKern(glyphCode2, &glyphCode3, &k)) {
                    glyphCode2 = glyphCode3;
                    glyphCode3 =
                        (iter == line.end()) ? NO_GLYPH_CODE : fontData_->translate(*++iter);
                    someLig = true;
                }
                if (someLig) {
                    fontData_->getFace(faceIndex_)->ligKern(glyphCode1, &glyphCode2, &kern);
                }
            }

            bool lastWordChar = (glyphCode2 == SPACE_CODE) || (glyphCode2 == NO_GLYPH_CODE);
            (handler)(glyphCode1, kern, firstWordChar, lastWordChar);
            firstWordChar = false;
            if (lastWordChar) {
                wasEndOfWord = true;
            }
            glyphCode1 = glyphCode2;
            glyphCode2 = (iter == line.end()) ? NO_GLYPH_CODE : fontData_->translate(*iter++);
        }
    }
}

// Returns the x position at the end of string
auto Font::drawSingleLineOfText(ibmf_defs::Bitmap &canvas, ibmf_defs::Pos pos,
                                const std::string &line, bool inverted) -> int {

    ibmf_defs::Pos atPos = pos;

    if constexpr (IBMF_TRACING) {
        LOGD("drawSingleLineOfText()");
    }

    if (isInitialized()) {

        // We set here the canvas pitch value, as there is still some definitions required at the
        // application-level in regard of the upcoming Sol Glasse augmented resolution.
        //
        // The following may require some modification as the next Sol Glasses version
        // may be using a different pitch than the one computed here.

        canvas.pitch = (getDisplayPixelResolution() == PixelResolution::ONE_BIT)
                           ? (canvas.dim.width + 7) >> 3
                           : canvas.dim.width;

        ligKernUTF8Map(line, [this, &canvas, &atPos, inverted](GlyphCode glyphCode, FIX16 kern,
                                                               bool first, bool last) {
            int8_t hOffset = first ? fontData_->getFace(faceIndex_)->getGlyphHOffset(glyphCode) : 0;
            atPos.x += hOffset;

            if (glyphCode == SPACE_CODE) {
                atPos.x += fontData_->getFace(faceIndex_)->getSpaceSize();
            } else {
                std::optional<const Glyph *> glyph = fontData_->glyphCache_.getGlyph(
                    *fontData_->getFace(faceIndex_), glyphCode, faceIndex_);

                if (glyph.has_value()) {
                    if (glyph.value()->bitmap.pixels != nullptr) {
                        lastGlyphWidth_ = glyph.value()->bitmap.dim.width;
                        Pos outPos = Pos(atPos.x - glyph.value()->metrics.xoff,
                                         atPos.y - glyph.value()->metrics.yoff);
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

    return atPos.x;
}

auto Font::getTextSize(const std::string &buffer) -> ibmf_defs::Dim {

    if constexpr (IBMF_TRACING) {
        LOGD("getTextSize()");
    }
    ibmf_defs::Dim dim = ibmf_defs::Dim(0, 0);
    int16_t up = 0;
    int16_t down = 0;
    if (isInitialized()) {
        ligKernUTF8Map(buffer, [this, &dim, &up, &down](GlyphCode glyphCode, FIX16 kern, bool first,
                                                        bool last) {
            if (glyphCode == SPACE_CODE) {
                dim.width += fontData_->getFace(faceIndex_)->getSpaceSize();
            } else {
                std::optional<const Glyph *> glyph = fontData_->glyphCache_.getGlyph(
                    *fontData_->getFace(faceIndex_), glyphCode, faceIndex_);

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

    return dim;
}

auto Font::getTextWidth(const std::string &buffer) -> int {

    int width = 0;
    if (isInitialized()) {
        ligKernUTF8Map(buffer,
                       [this, &width](GlyphCode glyphCode, FIX16 kern, bool first, bool last) {
            if (glyphCode == SPACE_CODE) {
                width += fontData_->getFace(faceIndex_)->getSpaceSize();
            } else {
                std::optional<const Glyph *> glyph = fontData_->glyphCache_.getGlyph(
                    *fontData_->getFace(faceIndex_), glyphCode, faceIndex_);

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
    if constexpr (IBMF_TRACING) {
        LOGD("getTextHeight()");
    }
    int height = 0;
    if (isInitialized()) {
        Dim dim = getTextSize(buffer);
        height = dim.height;
    }

    return height;
}

#endif