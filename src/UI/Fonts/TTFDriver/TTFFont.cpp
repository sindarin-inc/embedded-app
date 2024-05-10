#include "config.h"

#if TTF_SUPPORT

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

    // if ((codePoint == ' ') || (codePoint == 0xA0) || (codePoint == 0x202F) ||
    //     ((codePoint >= 0x2000) && (codePoint <= 0x200F))) {
    //     glyphCode = SPACE_CODE;
    // } else if (codePoint == ZERO_WIDTH_CODEPOINT) {
    //     glyphCode = ZERO_WIDTH_CODE;
    // } else if ((codePoint >= 0xFFF0) && (codePoint <= 0xFFFF)) {
    //     glyphCode = DONT_CARE_CODE;
    // } else {
    //     glyphCode = FT_Get_Char_Index(face_, codePoint);
    // }

    glyphCode = FT_Get_Char_Index(face_, codePoint);

    // The following test could generates many entries in the log, depending on the quantity of
    // unknown code points received to be translated. Could be removed or disabled if required.
    if ((codePoint != UNKNOWN_CODEPOINT) && (glyphCode == unknownGlyphCode_)) {
        LOGW("Unknown Code Point received: U+%05" PRIx32, uint32_t(codePoint));
    }

    return glyphCode;
}

auto Font::ligKern(const GlyphCode glyphCode1, GlyphCode *glyphCode2, FIX16 *kern) const -> bool {

    // ToDo
    *kern = 0;
    return false;

    // if ((glyphCode1 == NO_GLYPH_CODE) || (*glyphCode2 == NO_GLYPH_CODE)) {
    //     *kern = 0;
    //     return false;
    // }

    // // Check for ligatures
    // uint16_t lkIdx = getLigKernPgmIndex(glyphCode1);
    // if (lkIdx != NO_LIG_KERN_PGM) {
    //     LigKernStep *lk = getLigKernStep(lkIdx);
    //     if (lk->b.goTo.isAKern && lk->b.goTo.isAGoTo) {
    //         lkIdx = lk->b.goTo.displacement;
    //         lk = getLigKernStep(lkIdx);
    //     }

    //     GlyphCode code = (*glyphsInfo_)[*glyphCode2].mainCode;

    //     bool first = true;

    //     do {
    //         if (!first) {
    //             lk++;
    //         } else {
    //             first = false;
    //         }

    //         if (lk->a.nextGlyphCode == code) {
    //             if (lk->b.kern.isAKern) {
    //                 *kern = lk->b.kern.kerningValue;
    //                 return false; // No other iteration to be done
    //             } else {
    //                 *glyphCode2 = lk->b.repl.replGlyphCode;
    //                 return true;
    //             }
    //         }
    //     } while (!lk->a.stop);
    // }

    // if (resolution_ == PixelResolution::EIGHT_BITS) {
    //     *kern = 1;
    //     return false;
    // }

    // // LOGD("Optical Kerning End");
    // return false;
}

void Font::copyBitmap(Bitmap &to, Bitmap &from, Pos atPos, bool inverted) {
    auto rowCount = from.dim.height;
    auto fromPtr = from.pixels;
    if (DEFAULT_RESOLUTION == font_defs::PixelResolution::ONE_BIT) {
        uint8_t data;
        uint8_t fromMask = 0;

        auto toPtr = to.pixels + static_cast<size_t>(atPos.y * to.dim.width);
        for (uint16_t fromRow = 0; fromRow < from.dim.height; fromRow++, toPtr += to.dim.width) {
            uint8_t toMask = 0x80 >> (atPos.x & 7);
            int idx = (atPos.x >> 3);
            if (inverted) {
                for (uint16_t i = 0; i < from.dim.width; i++) {
                    if (fromMask == 0) {
                        fromMask = 0x80;
                        data = *fromPtr++;
                    }
                    if (data & fromMask) {
                        toPtr[idx] |= toMask;
                    }
                    fromMask >>= 1;
                    toMask >>= 1;
                    if (toMask == 0) {
                        idx += 1;
                        toMask = 0x80;
                    }
                }
            } else {
                for (uint16_t i = 0; i < from.dim.width; i++) {
                    if (fromMask == 0) {
                        fromMask = 0x80;
                        data = *fromPtr++;
                    }
                    if (data & fromMask) {
                        toPtr[idx] &= ~toMask;
                    }
                    fromMask >>= 1;
                    toMask >>= 1;
                    if (toMask == 0) {
                        idx += 1;
                        toMask = 0x80;
                    }
                }
            }
        }
    } else { // HEIGHT_BITS
        auto toPtr = &to.pixels[to.dim.width * atPos.y + atPos.x];
        while (rowCount-- > 0) {
            if (inverted) {
                memcpy(toPtr, fromPtr, from.dim.width);
            } else {
                for (uint16_t i = 0; i < from.dim.width; i++) {
                    toPtr[i] = 255 - fromPtr[i];
                }
            }
            fromPtr += from.dim.width;
            toPtr += to.dim.width;
        }
    }
}

// Get a Glyph from the font.
//
// Parameters:
//
//   glyphCode  : The index in the font to retrieve the glyph from
//   appGlyph   : The Glyph structure to put the information in
//   loadBitmap : If true, the glyph bitmap needs to be retrieved and put in the appGlyph.
//                If false, no bitmap is retrieved and THE OTHER PARAMETERS ARE IGNORED
//   caching    : If true (default), a bitmap needs to be allocated and the Glyph bitmap must be put
//                                   there.
//                If false, the appGlyph.bitmap already point at the screen bitmap
//   atPos      : This is the location in the screen bitmap where the
//                glyph's bitmap must be located, default: [0, 0]
//   inverted   : If true, the pixels must be put in "reversed video", default:false

auto Font::getGlyph(GlyphCode glyphCode, Glyph &appGlyph, bool loadBitmap, bool caching, Pos atPos,
                    bool inverted) -> bool {

    // ToDo

    int error;

    if (caching) {
        appGlyph.clear();
    }

    error = FT_Load_Glyph(face_,     /* handle to face object */
                          glyphCode, /* glyph index           */
                          loadBitmap ? FT_LOAD_DEFAULT : FT_LOAD_NO_BITMAP); /* load flags */
    if (error) {
        LOGE("Unable to load glyph for charcode: %d", glyphCode);
        return false;
    }

    FT_GlyphSlot slot = face_->glyph;

    if (loadBitmap) {
        if (face_->glyph->format != FT_GLYPH_FORMAT_BITMAP) {
            if (DEFAULT_RESOLUTION == font_defs::PixelResolution::ONE_BIT) {
                error = FT_Render_Glyph(face_->glyph,         // glyph slot
                                        FT_RENDER_MODE_MONO); // render mode
            } else {
                error = FT_Render_Glyph(face_->glyph,           // glyph slot
                                        FT_RENDER_MODE_NORMAL); // render mode
            }

            if (error) {
                LOGE("Unable to render glyph for charcode: %d error: %d", glyphCode, error);
                return false;
            }
        }

        auto dim = Dim(slot->bitmap.width, slot->bitmap.rows);

        if (caching) {
            appGlyph.bitmap.dim = dim;
            // glyph->pitch = slot->bitmap.pitch;

            uint16_t size = (DEFAULT_RESOLUTION == PixelResolution::ONE_BIT)
                                ? dim.height * ((dim.width + 7) >> 3)
                                : dim.height * dim.width;
            if (size > 0) {
                appGlyph.bitmap.pixels = new (std::nothrow) uint8_t[size];
                if (appGlyph.bitmap.pixels == nullptr) {
                    LOGE("Unable to allocate glyph pixel memory of size %d!", size);
                    appGlyph.bitmap.dim = Dim(0, 0);
                    return false;
                }
                memset(appGlyph.bitmap.pixels, inverted ? 0 : 0xFF, size);
            }
        }

        Bitmap glyphBitmap = {.pixels = slot->bitmap.buffer,
                              .dim = Dim(slot->bitmap.width, slot->bitmap.rows)};

        Pos outPos = Pos(atPos.x + slot->bitmap_left, atPos.y - slot->bitmap_top);
        copyBitmap(appGlyph.bitmap, glyphBitmap, outPos, inverted);
    }

    appGlyph.metrics = {.xoff = static_cast<int16_t>(slot->bitmap_left),
                        .yoff = static_cast<int16_t>(-slot->bitmap_top),
                        .descent = static_cast<int16_t>((slot->bitmap.rows + slot->bitmap_top) > 0
                                                            ? slot->bitmap.rows + slot->bitmap_top
                                                            : 0),
                        .advance = static_cast<FIX16>(slot->advance.x),
                        .lineHeight = static_cast<int16_t>(face_->size->metrics.height >> 6)};
    return true;
}

auto Font::getGlyphMetrics(GlyphCode glyphCode, Glyph &appGlyph) -> bool {

    appGlyph.clear();

    auto error = FT_Load_Glyph(face_,     /* handle to face object */
                          glyphCode, /* glyph index           */
                          FT_LOAD_NO_BITMAP); /* load flags */
    if (error) {
        LOGE("Unable to load glyph for charcode: %d", glyphCode);
        return false;
    }

    FT_GlyphSlot slot = face_->glyph;

    appGlyph.metrics = {.xoff = static_cast<int16_t>(slot->bitmap_left),
                        .yoff = static_cast<int16_t>(-slot->bitmap_top),
                        .descent = static_cast<int16_t>((slot->bitmap.rows + slot->bitmap_top) > 0
                                                            ? slot->bitmap.rows + slot->bitmap_top
                                                            : 0),
                        .advance = static_cast<FIX16>(slot->advance.x),
                        .lineHeight = static_cast<int16_t>(face_->size->metrics.height >> 6)};
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

void Font::drawSingleLineOfText(font_defs::Bitmap &canvas, font_defs::Pos pos,
                                const std::string &line, bool inverted) {
    if constexpr (TTF_TRACING) {
        LOGD("drawSingleLineOfText()");
    }
    if (isInitialized()) {
        font_defs::Pos atPos = pos;
        Glyph glyph{};
        glyph.bitmap = canvas;

        ligKernUTF8Map(line, [this, &glyph, &atPos, inverted](GlyphCode glyphCode, FIX16 kern,
                                                              bool first, bool last) {
            int8_t hOffset = first ? getGlyphHOffset(glyphCode) : 0;
            atPos.x += hOffset;

            // LOGD("Word Markers: %d %d", first, last);

            if (getGlyph(glyphCode, glyph, true, false, atPos, inverted)) {
                // As advance is positive and greather than kern, we can shift right
                // to get rid of the fix point decimals
                if (glyphCode == SPACE_CODE) {
                    atPos.x += glyph.metrics.advance >> 6;
                } else {
                    atPos.x += last ? getGlyphWidth(glyphCode) - (kern / 64) - glyph.metrics.xoff
                                    : ((glyph.metrics.advance + kern) >> 6);
                }
            }
        });
    }
}

auto Font::getTextSize(const std::string &buffer) -> font_defs::Dim {
    // LOGD("getTextSize(): %s", buffer.c_str());
    if constexpr (TTF_TRACING) {
        LOGD("getTextSize()");
    }
    font_defs::Dim dim = font_defs::Dim(0, 0);
    int16_t up = 0;
    int16_t down = 0;
    if (isInitialized()) {
        ligKernUTF8Map(buffer, [this, &dim, &up, &down](GlyphCode glyphCode, FIX16 kern, bool first,
                                                        bool last) {
            // int8_t hOffset = first ? fontData_->getFace(faceIndex_)->getGlyphHOffset(glyphCode) :
            // 0;

            font_defs::Glyph glyph{};
            if (getGlyph(glyphCode, glyph, false)) { // retrieves only the metrics
                // LOGD("Advance: %f, xoff: %d, yoff: %d, descent: %d, kern: %d",
                //      IBMFFaceLow::fromFIX16(glyph.metrics.advance), glyph.metrics.xoff,
                //      glyph.metrics.yoff, glyph.metrics.descent, kern);
                if (glyphCode == SPACE_CODE) {
                    dim.width += glyph.metrics.advance >> 6;
                } else {
                    dim.width += last ? getGlyphWidth(glyphCode) - (kern / 64) - glyph.metrics.xoff
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
    if constexpr (TTF_TRACING) {
        LOGD("getTextWidth()");
    }
    int width = 0;
    if (isInitialized()) {
        ligKernUTF8Map(buffer,
                       [this, &width](GlyphCode glyphCode, FIX16 kern, bool first, bool last) {
            font_defs::Glyph glyph{};
            if (getGlyph(glyphCode, glyph, false)) { // retrieves only the metrics

                // LOGD("Advance: %f, xoff: %d, yoff: %d, descent: %d, kern: %d",
                //      IBMFFaceLow::fromFIX16(glyph.metrics.advance), glyph.metrics.xoff,
                //      glyph.metrics.yoff, glyph.metrics.descent, kern);

                if (glyphCode == SPACE_CODE) {
                    width += glyph.metrics.advance >> 6;
                } else {
                    width += last ? getGlyphWidth(glyphCode) - (kern / 64) - glyph.metrics.xoff
                                  : ((glyph.metrics.advance + kern) >> 6); // + glyph.metrics.xoff;
                }

                // LOGD("Advance value: %f, kern: %f",
                //     IBMFFaceLow::fromFIX16(glyph.metrics.advance),
                //     IBMFFaceLow::fromFIX16(kern));
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
    if constexpr (TTF_TRACING) {
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