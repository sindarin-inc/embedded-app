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

    if ((codePoint == ' ') || (codePoint == 0xA0) || (codePoint == 0x202F) ||
        ((codePoint >= 0x2000) && (codePoint <= 0x200F))) {
        glyphCode = SPACE_CODE;
    } else if (codePoint == ZERO_WIDTH_CODEPOINT) {
        glyphCode = ZERO_WIDTH_CODE;
    } else if ((codePoint >= 0xFFF0) && (codePoint <= 0xFFFF)) {
        glyphCode = DONT_CARE_CODE;
    } else {
        glyphCode = FT_Get_Char_Index(face_, codePoint);
    }

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

auto Font::getGlyph(GlyphCode glyphCode, Glyph &appGlyph, bool loadBitmap, bool caching, Pos atPos,
                    bool inverted) -> bool {

    // ToDo

    // LOGD("glyphCode: %04x, loadBitmap: %s, caching: %s, pos: [%d, %d]", glyphCode,
    //      loadBitmap ? "YES" : "NO", caching ? "YES" : "NO", atPos.x, atPos.y);

    // if (caching) {
    //     appGlyph.clear();
    // }

    // if ((glyphCode == SPACE_CODE) ||
    //     ((glyphCode < faceHeader_->glyphCount) &&
    //      ((*glyphsInfo_)[glyphCode].bitmapWidth == 0))) { // send as a space character
    //     appGlyph.pointSize = faceHeader_->pointSize;
    //     appGlyph.metrics = {
    //         .xoff = 0,
    //         .yoff = 0,
    //         .descent = 0,
    //         .advance = static_cast<FIX16>(static_cast<uint16_t>(faceHeader_->spaceSize) << 6),
    //         .lineHeight = faceHeader_->lineHeight,
    //         .ligatureAndKernPgmIndex = 255};
    //     // For some reason, casting this above triggers a narrowing conversion error but it
    //     // doesn't here.
    //     // appGlyph.metrics.advance =
    //     // static_cast<FIX16>(static_cast<uint16_t>(faceHeader_->spaceSize) << 6);
    //     return true;
    // }

    // if (glyphCode >= faceHeader_->glyphCount) {
    //     return false;
    // }

    // GlyphInfo *glyphInfo = &(*glyphsInfo_)[glyphCode];

    // if (glyphInfo == nullptr) {
    //     return false;
    // }

    // Dim dim = Dim(glyphInfo->bitmapWidth, glyphInfo->bitmapHeight);
    // Pos glyphOffsets = Pos(0, 0);

    // glyphOffsets.y = -glyphInfo->verticalOffset;
    // if (!caching) {
    //     glyphOffsets.x -= glyphInfo->horizontalOffset;
    // }

    // if (loadBitmap) {
    //     if (caching) {
    //         uint16_t size = (resolution_ == PixelResolution::ONE_BIT)
    //                             ? dim.height * ((dim.width + 7) >> 3)
    //                             : dim.height * dim.width;

    //         uint8_t white;
    //         if (resolution_ == PixelResolution::ONE_BIT) {
    //             white = WHITE_ONE_BIT ? 0xFF : 0;
    //         } else {
    //             white = WHITE_EIGHT_BITS;
    //         }

    //         appGlyph.bitmap.pixels = new (std::nothrow) uint8_t[size];
    //         if (appGlyph.bitmap.pixels == nullptr) {
    //             LOGE("Unable to allocate glyph pixel memory of size %d!", size);
    //             appGlyph.bitmap.dim = Dim(0, 0);
    //             return false;
    //         }
    //         memset(appGlyph.bitmap.pixels, white, size);
    //         appGlyph.bitmap.dim = dim;
    //     }

    //     RLEBitmap glyphBitmap = {.pixels =
    //     &(*pixelsPool_)[(*glyphsPixelPoolIndexes_)[glyphCode]],
    //                              .dim = Dim(glyphInfo->bitmapWidth, glyphInfo->bitmapHeight),
    //                              .length = glyphInfo->packetLength};
    //     RLEExtractor rle(resolution_);

    //     Pos outPos = Pos(atPos.x + glyphOffsets.x, atPos.y + glyphOffsets.y);
    //     // std::cout << "inPos: [" << atPos.x << ", " << atPos.y << "], outPos: [" << outPos.x
    //     //           << ", " << outPos.y << "] " << std::endl;
    //     rle.retrieveBitmap(glyphBitmap, appGlyph.bitmap, outPos, glyphInfo->rleMetrics,
    //     inverted); if (!caching) {
    //         showBitmap(appGlyph.bitmap);
    //     }
    // }

    // appGlyph.metrics = {
    //     .xoff = static_cast<int16_t>(glyphInfo->horizontalOffset),
    //     .yoff = static_cast<int16_t>(glyphInfo->verticalOffset),
    //     .descent = static_cast<int16_t>((glyphInfo->bitmapHeight - glyphInfo->verticalOffset) > 0
    //                                         ? glyphInfo->bitmapHeight - glyphInfo->verticalOffset
    //                                         : 0),
    //     .advance = glyphInfo->advance,
    //     .lineHeight = faceHeader_->lineHeight,
    //     .ligatureAndKernPgmIndex = glyphInfo->ligKernPgmIndex};

    // appGlyph.metrics = {
    //     .xoff = (int16_t) - (glyphOffsets.x + ((caching) ? glyphInfo->horizontalOffset : 0)),
    //     .yoff = (int16_t) - (glyphOffsets.y + ((caching) ? glyphInfo->verticalOffset : 0)),
    //     .advance = glyphInfo->advance,
    //     .lineHeight = faceHeader_->lineHeight,
    //     .ligatureAndKernPgmIndex = glyphInfo->ligKernPgmIndex};

    // showGlyph(appGlyph, glyphCode & 0xFF);
    return true;
}

auto Font::getGlyphMetrics(GlyphCode glyphCode, Glyph &appGlyph) -> bool {

    appGlyph.clear();

    // ToDo

    // if ((glyphCode == SPACE_CODE) ||
    //     ((glyphCode < faceHeader_->glyphCount) &&
    //      ((*glyphsInfo_)[glyphCode].bitmapWidth == 0))) { // send as a space character
    //     appGlyph.pointSize = faceHeader_->pointSize;
    //     appGlyph.metrics = {
    //         .xoff = 0,
    //         .yoff = 0,
    //         .descent = 0,
    //         .advance = static_cast<FIX16>(static_cast<uint16_t>(faceHeader_->spaceSize) << 6),
    //         .lineHeight = faceHeader_->lineHeight,
    //         .ligatureAndKernPgmIndex = 255};

    //     return true;
    // }

    // if (glyphCode >= faceHeader_->glyphCount) {
    //     return false;
    // }

    // GlyphInfo *glyphInfo = &(*glyphsInfo_)[glyphCode];

    // if (glyphInfo == nullptr) {
    //     return false;
    // }

    // appGlyph.metrics = {
    //     .xoff = static_cast<int16_t>(glyphInfo->horizontalOffset),
    //     .yoff = static_cast<int16_t>(glyphInfo->verticalOffset),
    //     .descent = static_cast<int16_t>((glyphInfo->bitmapHeight - glyphInfo->verticalOffset) > 0
    //                                         ? glyphInfo->bitmapHeight - glyphInfo->verticalOffset
    //                                         : 0),
    //     .advance = glyphInfo->advance,
    //     .lineHeight = faceHeader_->lineHeight,
    //     .ligatureAndKernPgmIndex = glyphInfo->ligKernPgmIndex};

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