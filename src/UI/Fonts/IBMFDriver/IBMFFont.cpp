#include "config.h"

#if IBMF_SUPPORT

#include "IBMFFont.hpp"

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

void Font::drawSingleLineOfText(ibmf_defs::Bitmap &canvas, ibmf_defs::Pos pos,
                                const std::string &line, bool inverted) const {
    if constexpr (IBMF_TRACING) {
        LOGD("drawSingleLineOfText()");
    }
    if (isInitialized()) {
        ibmf_defs::Pos atPos = pos;
        Glyph glyph{};
        glyph.bitmap = canvas;

        ligKernUTF8Map(line, [this, &glyph, &atPos, inverted](GlyphCode glyphCode, FIX16 kern,
                                                              bool first, bool last) {
            int8_t hOffset = first ? fontData_->getFace(faceIndex_)->getGlyphHOffset(glyphCode) : 0;
            atPos.x += hOffset;

            // LOGD("Word Markers: %d %d", first, last);

            if (fontData_->getFace(faceIndex_)
                    ->getGlyph(glyphCode, glyph, true, false, atPos, inverted)) {
                // As advance is positive and greather than kern, we can shift right
                // to get rid of the fix point decimals
                if (glyphCode == SPACE_CODE) {
                    atPos.x += glyph.metrics.advance >> 6;
                } else {
                    atPos.x += last ? fontData_->getFace(faceIndex_)->getGlyphWidth(glyphCode) -
                                          (kern / 64) - glyph.metrics.xoff
                                    : ((glyph.metrics.advance + kern) >> 6);
                }
            }
        });
    }
}

auto Font::getTextSize(const std::string &buffer) -> ibmf_defs::Dim {
    // LOGD("getTextSize(): %s", buffer.c_str());
    if constexpr (IBMF_TRACING) {
        LOGD("getTextSize()");
    }
    ibmf_defs::Dim dim = ibmf_defs::Dim(0, 0);
    int16_t up = 0;
    int16_t down = 0;
    if (isInitialized()) {
        ligKernUTF8Map(buffer, [this, &dim, &up, &down](GlyphCode glyphCode, FIX16 kern, bool first,
                                                        bool last) {
            // int8_t hOffset = first ? fontData_->getFace(faceIndex_)->getGlyphHOffset(glyphCode) :
            // 0;

            ibmf_defs::Glyph glyph{};
            if (fontData_->getFace(faceIndex_)
                    ->getGlyph(glyphCode, glyph, false)) { // retrieves only the metrics
                // LOGD("Advance: %f, xoff: %d, yoff: %d, descent: %d, kern: %d",
                //      IBMFFace::fromFIX16(glyph.metrics.advance), glyph.metrics.xoff,
                //      glyph.metrics.yoff, glyph.metrics.descent, kern);
                if (glyphCode == SPACE_CODE) {
                    dim.width += glyph.metrics.advance >> 6;
                } else {
                    dim.width += last ? fontData_->getFace(faceIndex_)->getGlyphWidth(glyphCode) -
                                            (kern / 64) - glyph.metrics.xoff
                                      : ((glyph.metrics.advance + kern) >> 6);
                }
                up = (up < glyph.metrics.yoff) ? glyph.metrics.yoff : up;
                down = (down < glyph.metrics.descent) ? glyph.metrics.descent : down;
                // dim.height = (dim.height > glyph.metrics.yoff)
                //                  ? glyph.metrics.yoff
                //                  : dim.height; // yoff is negative...
            } else {
                // LOGW("Unable to retrieve glyph for glyphCode %d", glyphCode);
            }
        });
    }
    dim.height = (up + down);
    // LOGD("IBMFFont: TextBound: width: %d, height: %d", dim.width, dim.height);
    return dim;
}

auto Font::getTextWidth(const std::string &buffer) -> int {
    if constexpr (IBMF_TRACING) {
        LOGD("getTextWidth()");
    }
    int width = 0;
    if (isInitialized()) {
        ligKernUTF8Map(buffer,
                       [this, &width](GlyphCode glyphCode, FIX16 kern, bool first, bool last) {
            ibmf_defs::Glyph glyph{};
            if (fontData_->getFace(faceIndex_)
                    ->getGlyph(glyphCode, glyph, false)) { // retrieves only the metrics

                // LOGD("Advance: %f, xoff: %d, yoff: %d, descent: %d, kern: %d",
                //      IBMFFace::fromFIX16(glyph.metrics.advance), glyph.metrics.xoff,
                //      glyph.metrics.yoff, glyph.metrics.descent, kern);

                if (glyphCode == SPACE_CODE) {
                    width += glyph.metrics.advance >> 6;
                } else {
                    width += last ? fontData_->getFace(faceIndex_)->getGlyphWidth(glyphCode) -
                                        (kern / 64) - glyph.metrics.xoff
                                  : ((glyph.metrics.advance + kern) >> 6); // + glyph.metrics.xoff;
                }

                // LOGD("Advance value: %f, kern: %f",
                //     IBMFFace::fromFIX16(glyph.metrics.advance),
                //     IBMFFace::fromFIX16(kern));
                // width += (glyph.metrics.advance + kern + 32) >> 6;
            } else {
                // LOGW("Unable to retrieve glyph for glyphCode %d", glyphCode);
            }
        });
    }
    // LOGD("IBMFFont: TextWidth: %d", width);
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

// auto IBMFFont::getTextWidthQuick(const char *buffer) -> int {
//     if constexpr (IBMF_TRACING) {
//         LOGD("getTextWidthQuick()");
//     }

//     int16_t width = 0;
//     auto face = fontData_->getFace(faceIndex_);

//     log_w("Buffer: %s", buffer);

//     while (*buffer) {
//         int16_t xoff;
//         int16_t approxWidth;
//         GlyphCode glyphCode;

//         if (face->getGlyphApproxWidth(glyphCode = fontData_->translate(toChar32(&buffer)),
//                                       &approxWidth)) {
//             width += (*buffer == '\0') ? approxWidth - 1 : approxWidth;
//         }
//     }
//     log_w("Width: %" PRIi16 " + %" PRIi16, width, (width >> 3) + 1);
//     return width + (width >> 3) + 1;
// }

auto Font::getTextHeight(const std::string &buffer) -> int {
    if constexpr (IBMF_TRACING) {
        LOGD("getTextHeight()");
    }
    int height = 0;
    if (isInitialized()) {
        // for (UTF8Iterator chrIter = buffer.begin(); chrIter != buffer.end(); chrIter++) {
        //     ibmf_defs::Glyph glyph;
        //     if (fontData_->getFace(faceIndex_)->getGlyph(*chrIter, glyph, false)) {
        //         // LOGD("yoff value: %d", glyph.metrics.yoff);
        //         if (height > glyph.metrics.yoff) {
        //             height = glyph.metrics.yoff; // yoff is negative
        //         }
        //     } else {
        //         LOGW("Unable to retrieve glyph for char %d(%c)", *chrIter, *chrIter);
        //     }
        // }
        // height = fontData_->getFace(faceIndex_)->getEmHeight();
        Dim dim = getTextSize(buffer);
        height = dim.height;
    }
    // LOGD("TextHeight: %d", height);
    return height;
}

#endif