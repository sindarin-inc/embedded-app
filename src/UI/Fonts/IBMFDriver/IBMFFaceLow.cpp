#include "IBMFFaceLow.hpp"

auto IBMFFaceLow::load(const MemoryPtr dataStart, const int dataLength, FontFormat fontFmt)
    -> bool {

    MemoryPtr memoryPtr = dataStart;
    MemoryPtr memoryEnd = dataStart + dataLength;

    faceHeader_ = reinterpret_cast<FaceHeaderPtr>(memoryPtr);
    fontFormat_ = fontFmt;

    if (faceHeader_->glyphCount >= UTF32_MAX_GLYPH_COUNT) {
        LOGE("Too many glyphs in face. Maximum is %d in ibmf_defs.h", UTF32_MAX_GLYPH_COUNT);
        return false;
    }

    memoryPtr += sizeof(FaceHeader);
    glyphsPixelPoolIndexes_ = reinterpret_cast<GlyphsPixelPoolIndexes>(memoryPtr);

    memoryPtr += (sizeof(PixelPoolIndex) * faceHeader_->glyphCount);
    glyphsInfo_ = reinterpret_cast<GlyphsInfoPtr>(memoryPtr);

    memoryPtr += (sizeof(GlyphInfo) * faceHeader_->glyphCount);
    pixelsPool_ = reinterpret_cast<PixelsPoolPtr>(memoryPtr);

    memoryPtr += faceHeader_->pixelsPoolSize;
    ligKernSteps_ = reinterpret_cast<LigKernStepsPtr>(memoryPtr);

    memoryPtr += (sizeof(LigKernStep) * faceHeader_->ligKernStepCount);

    if (memoryPtr != memoryEnd) {
        LOGE("IBMFFace: Memory synch issue reading lig/kern struct!!");
        return false;
    }

    // showFace();
    initialized_ = true;
    return true;
}

/// @brief Search Ligature and Kerning table
///
/// Using the LigKern program of **glyphCode1**, find the first entry in the
/// program for which **glyphCode2** is the next character. If a ligature is
/// found, sets **glyphCode2** with the new code and returns *true*. If a
/// kerning entry is found, it sets the kern parameter with the value
/// in the table and return *false*. If the LigKern pgm is empty or there
/// is no entry for **glyphCode2**, it returns *false*.
///
/// Note: character codes have to be translated to internal GlyphCode before
/// calling this method.
///
/// @param glyphCode1 In. The GlyhCode for which to find a LigKern entry in its program.
/// @param glyphCode2 InOut. The GlyphCode that must appear in the program as the next
///                   character in sequence. Will be replaced with the target
///                   ligature GlyphCode if found.
/// @param kern Out. When a kerning entry is found in the program, kern will receive the value.
/// @return True if a ligature was found, false otherwise.
///
auto IBMFFaceLow::ligKern(const GlyphCode glyphCode1, GlyphCode *glyphCode2, FIX16 *kern) -> bool {

    if ((glyphCode1 >= faceHeader_->glyphCount) || (*glyphCode2 >= faceHeader_->glyphCount)) {
        *kern = 0;
        return false;
    }

    // Check for ligatures
    uint16_t lkIdx = getLigKernPgmIndex(glyphCode1);
    if (lkIdx != NO_LIG_KERN_PGM) {
        LigKernStep *lk = getLigKernStep(lkIdx);
        if (lk->b.goTo.isAKern && lk->b.goTo.isAGoTo) {
            lkIdx = lk->b.goTo.displacement;
            lk = getLigKernStep(lkIdx);
        }

        GlyphCode code = (*glyphsInfo_)[*glyphCode2].mainCode;

        bool first = true;

        do {
            if (!first) {
                lk++;
            } else {
                first = false;
            }

            if (lk->a.nextGlyphCode == code) {
                if (lk->b.kern.isAKern) {
                    *kern = lk->b.kern.kerningValue;
                    return false; // No other iteration to be done
                } else {
                    *glyphCode2 = lk->b.repl.replGlyphCode;
                    return true;
                }
            }
        } while (!lk->a.stop);
    }

    // TODO: Implement optical kerning for 8-bits resolution (GT)
    if (resolution_ == PixelResolution::EIGHT_BITS) {
        *kern = 1;
        return false;
    }

#if OPTICAL_KERNING

    typedef int32_t FIX32;

#define FRACT_BITS 10
#define FIXED_POINT_ONE (1 << FRACT_BITS)
#define MAKE_INT_FIXED(x) (static_cast<FIX32>((x) << FRACT_BITS))
#define MAKE_FLOAT_FIXED(x) (static_cast<FIX32>((x)*FIXED_POINT_ONE))
#define MAKE_FIXED_INT(x) ((x) >> FRACT_BITS)
#define MAKE_FIXED_FLOAT(x) ((static_cast<float>(x)) / FIXED_POINT_ONE)

#define FIXED_MULT(x, y) ((x) * (y) >> FRACT_BITS)
#define FIXED_DIV(x, y) (((x) << FRACT_BITS) / (y))

    auto &i1 = (*glyphsInfo_)[glyphCode1];
    auto &i2 = (*glyphsInfo_)[*glyphCode2];

    int normalDistance =
        i1.horizontalOffset + ((i1.advance + 32) >> 6) - i1.bitmapWidth - i2.horizontalOffset;

    // std::cout << "Normal distance:" << normal_distance << std::endl;

    int8_t origin = std::max(i1.verticalOffset, i2.verticalOffset);

    // start positions in each dist arrays
    uint8_t distIdxLeft = origin - i1.verticalOffset;
    uint8_t distIdxRight = origin - i2.verticalOffset;

    // idx and length in each bitmaps to compare
    uint8_t firstIdxLeft = origin - i2.verticalOffset;
    uint8_t firstIdxRight = origin - i1.verticalOffset;
    int8_t length =
        std::min((static_cast<int8_t>(i1.bitmapHeight) - static_cast<int8_t>(firstIdxLeft)),
                 (static_cast<int8_t>(i2.bitmapHeight) - static_cast<int8_t>(firstIdxRight)));
    uint8_t firstIdx = std::max(firstIdxLeft, firstIdxRight);

    FIX32 kerning = 0;

    // if (length > 0) { // Length <= 0 means that there is no alignment between the characters
    Glyph glyph1{};
    bool result = getGlyph(glyphCode1, glyph1, true, true, Pos(0, i1.verticalOffset));
    if (!result) {

        delete[] glyph1.bitmap.pixels;

        LOGE("Unable to load glyphCode %d related glyph bitmap.", glyphCode1);
        return false;
    }

    Glyph glyph2{};
    result = getGlyph(*glyphCode2, glyph2, true, true, Pos(0, i2.verticalOffset));
    if (!result) {

        delete[] glyph1.bitmap.pixels;

        delete[] glyph2.bitmap.pixels;

        LOGE("Unable to load glyphCode %d related glyph bitmap.", *glyphCode2);
        return false;
    }

    // hight of significant parts of dist arrays
    int8_t hight = origin + std::max((static_cast<int8_t>(i1.bitmapHeight) - i1.verticalOffset),
                                     (static_cast<int8_t>(i2.bitmapHeight) - i2.verticalOffset));

    // distance computation for left and right characters
    auto distLeft = std::shared_ptr<FIX32[]>(new FIX32[hight]);
    auto distRight = std::shared_ptr<FIX32[]>(new FIX32[hight]);

    for (int i = 0; i < hight; i++) {
        distLeft[i] = MAKE_FLOAT_FIXED(-1.0);
        distRight[i] = MAKE_FLOAT_FIXED(-1.0);
    }

    // distLeft is receiving the right distance in pixels of the first black pixel on each
    // line of the character
    int idx = 0;
    int pitch = (i1.bitmapWidth + 7) >> 3;
    for (uint8_t i = distIdxLeft; i < i1.bitmapHeight + distIdxLeft; i++, idx += pitch) {
        distLeft[i] = 0;
        uint8_t mask = 1 << (7 - ((i1.bitmapWidth - 1) & 7));
        uint8_t *p = &glyph1.bitmap.pixels[idx + ((i1.bitmapWidth - 1) >> 3)];
        for (int col = i1.bitmapWidth - 1; col >= 0; col--) {
            if constexpr (BLACK_ONE_BIT) {
                if ((*p & mask) != 0) {
                    break;
                }
            } else {
                if ((*p & mask) == 0) {
                    break;
                }
            }
            distLeft[i] += FIXED_POINT_ONE;
            mask <<= 1;
            if (mask == 0) {
                mask = 0x01;
                p -= 1;
            }
        }
    }

    // distRight is receiving the left distance in pixels of the first black pixel on each
    // line of the character
    idx = 0;
    pitch = (i2.bitmapWidth + 7) >> 3;
    for (uint8_t i = distIdxRight; i < i2.bitmapHeight + distIdxRight; i++, idx += pitch) {
        distRight[i] = 0;
        uint8_t mask = 0x80;
        uint8_t *p = &glyph2.bitmap.pixels[idx];
        for (int col = 0; col < i2.bitmapWidth; col++) {
            if constexpr (BLACK_ONE_BIT) {
                if ((*p & mask) != 0) {
                    break;
                }
            } else {
                if ((*p & mask) == 0) {
                    break;
                }
            }
            distRight[i] += FIXED_POINT_ONE;
            mask >>= 1;
            if (mask == 0) {
                mask = 0x80;
                p += 1;
            }
        }
    }

    // find convex corner locations and adjust distances

    // Right Side Convex Hull for the character at left
    if (i1.bitmapHeight >= 3) { // 1 and 2 line characters don't need adjustment

        // Compute the cross product of 3 points. If negative, the angle is convex
        auto crossLeft = [distLeft](int i, int j, int k) -> FIX32 {
            return FIXED_MULT((distLeft[j] - distLeft[i]), MAKE_INT_FIXED(k - i)) -
                   FIXED_MULT(MAKE_INT_FIXED(j - i), (distLeft[k] - distLeft[i]));
        };

        // Adjusts distances to get a line between two vertices of the Convex Hull
        auto adjustLeft = [distLeft](int i, int j) {
            if ((j - i) > 1) {
                if (abs(distLeft[j] - distLeft[i]) <= MAKE_FLOAT_FIXED(0.01)) {
                    for (int k = i + 1; k < j; k++) {
                        distLeft[k] = distLeft[i];
                    }
                } else {
                    FIX32 slope = FIXED_DIV((distLeft[j] - distLeft[i]), MAKE_INT_FIXED(j - i));
                    FIX32 v = distLeft[i];
                    for (int k = i + 1; k < j; k++) {
                        v += slope;
                        distLeft[k] = v;
                    }
                }
            }
        };

        // Find vertices using the cross product and adjust the distances
        // to get the right portion of the Convex Hull polygon.
        int i = static_cast<int>(distIdxLeft);
        int j = i + 1;
        while (j < (i1.bitmapHeight + distIdxLeft)) {
            bool found = true;
            for (int k = j + 1; k < i1.bitmapHeight + distIdxLeft; k++) {
                FIX32 val = crossLeft(i, j, k);
                if (val >= 0) {
                    found = false;
                    break;
                }
            }
            if (found) {
                adjustLeft(i, j);
                i = j;
                j = i + 1;
            } else {
                j += 1;
            }
        }
    }

    // Left side Convex Hull for the character at right

    if (i2.bitmapHeight >= 3) { // 1 and 2 line characters don't need adjustment

        // Compute the cross product of 3 points. If negative, the angle is convex.
        auto crossRight = [distRight](int i, int j, int k) -> FIX32 {
            return FIXED_MULT((distRight[j] - distRight[i]), MAKE_INT_FIXED(k - i)) -
                   FIXED_MULT(MAKE_INT_FIXED(j - i), (distRight[k] - distRight[i]));
        };

        // Adjusts distances to get a line between two vertices of the Convex Hull
        auto adjustRight = [distRight](int i, int j) {
            if ((j - i) > 1) {
                if (abs(distRight[j] - distRight[i]) <= MAKE_FLOAT_FIXED(0.01)) {
                    for (int k = i + 1; k < j; k++) {
                        distRight[k] = distRight[i];
                    }
                } else {
                    FIX32 slope = FIXED_DIV((distRight[j] - distRight[i]), MAKE_INT_FIXED(j - i));
                    FIX32 v = distRight[i];
                    for (int k = i + 1; k < j; k++) {
                        v += slope;
                        distRight[k] = v;
                    }
                }
            }
        };

        // Find vertices using the cross product and adjust the distances
        // to get the left portion of the Convex Hull polygon.
        int i = static_cast<int>(distIdxRight);
        int j = i + 1;
        while (j < (i2.bitmapHeight + distIdxRight)) {
            bool found = true;
            for (int k = j + 1; k < i2.bitmapHeight + distIdxRight; k++) {
                FIX32 val = crossRight(i, j, k);
                if (val >= 0) {
                    found = false;
                    break;
                }
            }
            if (found) {
                adjustRight(i, j);
                i = j;
                j = i + 1;
            } else {
                j += 1;
            }
        }
    }

    if (length <= 0) {
        if (distIdxRight > distIdxLeft) {
            FIX32 val = distRight[distIdxRight];
            uint8_t i = distIdxRight - 1;
            while (length <= 0) {
                distRight[i--] = val;
                length += 1;
                firstIdx -= 1;
                distIdxRight -= 1;
            }
        } else {
            FIX32 val = distLeft[distIdxLeft];
            uint8_t i = distIdxLeft - 1;
            while (length <= 0) {
                distLeft[i--] = val;
                length += 1;
                firstIdx -= 1;
                distIdxLeft -= 1;
            }
        }
    }

    // Now, compute the smallest distance that exists between
    // the two characters. Pixels on each line are checked as well
    // as angled pixels (on the lines above and below)
    kerning = MAKE_INT_FIXED(999);
    FIX32 dist;

    for (uint8_t i = firstIdx; i < firstIdx + length; i++) {
        dist = distLeft[i] + distRight[i];
        if (dist < kerning) {
            kerning = dist;
        }
        if ((i > 0) && (distLeft[i - 1] >= 0)) {
            dist = distLeft[i - 1] + distRight[i];
            if (dist < kerning) {
                kerning = dist;
            }
        }
        if ((i < (hight - 1)) && (distLeft[i + 1] >= 0)) {
            dist = distLeft[i + 1] + distRight[i];
            if (dist < kerning) {
                kerning = dist;
            }
        }
    }
    if ((firstIdx > 0) && (distRight[firstIdx - 1] >= 0)) {
        dist = distLeft[firstIdx] + distRight[firstIdx - 1];
        if (dist < kerning) {
            kerning = dist;
        }
    }
    uint8_t lastIdx = firstIdx + length - 1;
    if ((lastIdx < (hight - 1)) && (distRight[lastIdx + 1] >= 0)) {
        dist = distLeft[lastIdx] + distRight[lastIdx + 1];
        if (dist < kerning) {
            kerning = dist;
        }
    }

    int addedWildcard;

    if (i2.rleMetrics.beforeAddedOptKern == 3) {
        addedWildcard = -1;
    } else {
        addedWildcard = i2.rleMetrics.beforeAddedOptKern;
    }
    addedWildcard += i1.rleMetrics.afterAddedOptKern;

    // std::cout << "Minimal distance: " << MAKE_FIXED_FLOAT(kerning) << std::endl;

    // Adjust the resulting kerning value, considering the targetted KERNING_SIZE (the space
    // to have between characters), the size of the character and the normal distance that
    // will be used by the writing algorithm
    kerning = (-std::min(kerning - MAKE_INT_FIXED(KERNING_SIZE + addedWildcard),
                         MAKE_INT_FIXED(i2.bitmapWidth))) -
              MAKE_INT_FIXED(normalDistance);

    delete[] glyph1.bitmap.pixels;

    delete[] glyph2.bitmap.pixels;

    // }

    *kern = static_cast<FIX16>(kerning >> 4); // Convert to FIX16
#endif
    // LOGD("Optical Kerning End");
    return false;
}

auto IBMFFaceLow::getGlyph(GlyphCode glyphCode, Glyph &appGlyph, bool loadBitmap, bool caching,
                           Pos atPos, bool inverted) -> bool {

    // LOGD("glyphCode: %04x, loadBitmap: %s, caching: %s, pos: [%d, %d]", glyphCode,
    //      loadBitmap ? "YES" : "NO", caching ? "YES" : "NO", atPos.x, atPos.y);

    if (caching) {
        appGlyph.clear();
    }

    if (fontFormat_ != FontFormat::UTF32) {
        LOGE("Unknown character set: %d", fontFormat_);
        return false;
    }

    if ((glyphCode == SPACE_CODE) ||
        ((glyphCode < faceHeader_->glyphCount) &&
         ((*glyphsInfo_)[glyphCode].bitmapWidth == 0))) { // send as a space character
        appGlyph.pointSize = faceHeader_->pointSize;
        appGlyph.metrics = {
            .xoff = 0,
            .yoff = 0,
            .descent = 0,
            .advance = static_cast<FIX16>(static_cast<uint16_t>(faceHeader_->spaceSize) << 6),
            .lineHeight = faceHeader_->lineHeight,
            .ligatureAndKernPgmIndex = 255};
        // For some reason, casting this above triggers a narrowing conversion error but it
        // doesn't here.
        // appGlyph.metrics.advance =
        // static_cast<FIX16>(static_cast<uint16_t>(faceHeader_->spaceSize) << 6);
        return true;
    }

    if (glyphCode >= faceHeader_->glyphCount) {
        return false;
    }

    GlyphInfo *glyphInfo = &(*glyphsInfo_)[glyphCode];

    if (glyphInfo == nullptr) {
        return false;
    }

    Dim dim = Dim(glyphInfo->bitmapWidth, glyphInfo->bitmapHeight);
    Pos glyphOffsets = Pos(0, 0);

    glyphOffsets.y = -glyphInfo->verticalOffset;
    if (!caching) {
        glyphOffsets.x -= glyphInfo->horizontalOffset;
    }

    if (loadBitmap) {
        if (caching) {
            uint16_t size = (resolution_ == PixelResolution::ONE_BIT)
                                ? dim.height * ((dim.width + 7) >> 3)
                                : dim.height * dim.width;

            uint8_t white;
            if (resolution_ == PixelResolution::ONE_BIT) {
                white = WHITE_ONE_BIT ? 0xFF : 0;
            } else {
                white = WHITE_EIGHT_BITS;
            }

            appGlyph.bitmap.pixels = new (std::nothrow) uint8_t[size];
            if (appGlyph.bitmap.pixels == nullptr) {
                LOGE("Unable to allocate glyph pixel memory of size %d!", size);
                appGlyph.bitmap.dim = Dim(0, 0);
                return false;
            }
            memset(appGlyph.bitmap.pixels, white, size);
            appGlyph.bitmap.dim = dim;
        }

        RLEBitmap glyphBitmap = {.pixels = &(*pixelsPool_)[(*glyphsPixelPoolIndexes_)[glyphCode]],
                                 .dim = Dim(glyphInfo->bitmapWidth, glyphInfo->bitmapHeight),
                                 .length = glyphInfo->packetLength};
        RLEExtractor rle(resolution_);

        Pos outPos = Pos(atPos.x + glyphOffsets.x, atPos.y + glyphOffsets.y);
        // std::cout << "inPos: [" << atPos.x << ", " << atPos.y << "], outPos: [" << outPos.x
        //           << ", " << outPos.y << "] " << std::endl;
        rle.retrieveBitmap(glyphBitmap, appGlyph.bitmap, outPos, glyphInfo->rleMetrics, inverted);
        if (!caching) {
            showBitmap(appGlyph.bitmap);
        }
    }

    appGlyph.metrics = {
        .xoff = static_cast<int16_t>(glyphInfo->horizontalOffset),
        .yoff = static_cast<int16_t>(glyphInfo->verticalOffset),
        .descent = static_cast<int16_t>((glyphInfo->bitmapHeight - glyphInfo->verticalOffset) > 0
                                            ? glyphInfo->bitmapHeight - glyphInfo->verticalOffset
                                            : 0),
        .advance = glyphInfo->advance,
        .lineHeight = faceHeader_->lineHeight,
        .ligatureAndKernPgmIndex = glyphInfo->ligKernPgmIndex};

    // appGlyph.metrics = {
    //     .xoff = (int16_t) - (glyphOffsets.x + ((caching) ? glyphInfo->horizontalOffset : 0)),
    //     .yoff = (int16_t) - (glyphOffsets.y + ((caching) ? glyphInfo->verticalOffset : 0)),
    //     .advance = glyphInfo->advance,
    //     .lineHeight = faceHeader_->lineHeight,
    //     .ligatureAndKernPgmIndex = glyphInfo->ligKernPgmIndex};

    // showGlyph(appGlyph, glyphCode & 0xFF);
    return true;
}

auto IBMFFaceLow::getGlyphMetrics(GlyphCode glyphCode, Glyph &appGlyph) -> bool {

    appGlyph.clear();

    if ((glyphCode == SPACE_CODE) ||
        ((glyphCode < faceHeader_->glyphCount) &&
         ((*glyphsInfo_)[glyphCode].bitmapWidth == 0))) { // send as a space character
        appGlyph.pointSize = faceHeader_->pointSize;
        appGlyph.metrics = {
            .xoff = 0,
            .yoff = 0,
            .descent = 0,
            .advance = static_cast<FIX16>(static_cast<uint16_t>(faceHeader_->spaceSize) << 6),
            .lineHeight = faceHeader_->lineHeight,
            .ligatureAndKernPgmIndex = 255};

        return true;
    }

    if (glyphCode >= faceHeader_->glyphCount) {
        return false;
    }

    GlyphInfo *glyphInfo = &(*glyphsInfo_)[glyphCode];

    if (glyphInfo == nullptr) {
        return false;
    }

    appGlyph.metrics = {
        .xoff = static_cast<int16_t>(glyphInfo->horizontalOffset),
        .yoff = static_cast<int16_t>(glyphInfo->verticalOffset),
        .descent = static_cast<int16_t>((glyphInfo->bitmapHeight - glyphInfo->verticalOffset) > 0
                                            ? glyphInfo->bitmapHeight - glyphInfo->verticalOffset
                                            : 0),
        .advance = glyphInfo->advance,
        .lineHeight = faceHeader_->lineHeight,
        .ligatureAndKernPgmIndex = glyphInfo->ligKernPgmIndex};

    return true;
}

// auto IBMFFaceLow::getGlyphApproxWidth(GlyphCode glyphCode, int16_t *approxWidth) -> bool {

//     if ((glyphCode == SPACE_CODE) || ((glyphCode < faceHeader_->glyphCount)
//         ((*glyphsInfo_)[glyphCode].bitmapWidth == 0))) { // send as a space character
//         *approxWidth = static_cast<int16_t>(faceHeader_->spaceSize);
//         return true;
//     }

//     if (glyphCode >= faceHeader_->glyphCount) {
//         return false;
//     }

//     GlyphInfo *glyphInfo = &(*glyphsInfo_)[glyphCode];

//     if (glyphInfo == nullptr) {
//         return false;
//     }

//     *approxWidth = glyphInfo->bitmapWidth + 1;

//     return true;
// }

auto IBMFFaceLow::showBitmap(const Bitmap &bitmap) const -> void {
    if constexpr (DEBUG) {
        uint32_t row, col;
        MemoryPtr rowPtr;

        uint32_t maxWidth = 50;
        if (bitmap.dim.width < maxWidth) {
            maxWidth = bitmap.dim.width;
        }

        std::cout << "   +";
        for (col = 0; col < maxWidth; col++) {
            std::cout << '-';
        }
        std::cout << '+' << std::endl << std::flush;

        if (resolution_ == PixelResolution::ONE_BIT) {
            uint32_t rowSize = (bitmap.dim.width + 7) >> 3;
            for (row = 0, rowPtr = bitmap.pixels; row < bitmap.dim.height;
                 row++, rowPtr += rowSize) {
                std::cout << "   |";
                for (col = 0; col < maxWidth; col++) {
                    if constexpr (BLACK_ONE_BIT) {
                        std::cout << ((rowPtr[col >> 3] & (0x80 >> (col & 7))) ? 'X' : ' ');
                    } else {
                        std::cout << ((rowPtr[col >> 3] & (0x80 >> (col & 7))) ? ' ' : 'X');
                    }
                }
                std::cout << '|';
                std::cout << std::endl << std::flush;
            }
        } else {
            uint32_t rowSize = bitmap.dim.width;
            for (row = 0, rowPtr = bitmap.pixels; row < bitmap.dim.height;
                 row++, rowPtr += rowSize) {
                std::cout << "   |";
                for (col = 0; col < maxWidth; col++) {
                    if constexpr (BLACK_EIGHT_BITS) {
                        std::cout << ((rowPtr[col] == BLACK_EIGHT_BITS) ? 'X' : ' ');
                    } else {
                        std::cout << ((rowPtr[col] == BLACK_EIGHT_BITS) ? ' ' : 'X');
                    }
                }
                std::cout << '|';
                std::cout << std::endl << std::flush;
            }
        }

        std::cout << "   +";
        for (col = 0; col < maxWidth; col++) {
            std::cout << '-';
        }
        std::cout << '+' << std::endl << std::endl << std::flush;
    }
}

auto IBMFFaceLow::showGlyph(const Glyph &glyph, GlyphCode glyphCode, char32_t codePoint) const
    -> void {
    if constexpr (DEBUG) {
        std::cout << "Glyph Base Code: 0x" << std::hex << glyphCode << std::dec << "(" << +codePoint
                  << ")" << std::endl
                  << "  Metrics: [" << std::dec << glyph.bitmap.dim.width << ", "
                  << glyph.bitmap.dim.height << "] " << std::endl
                  << "  Position: [" << glyph.metrics.xoff << ", " << glyph.metrics.yoff << ']'
                  << std::endl
                  << "  Bitmap available: " << ((glyph.bitmap.pixels == nullptr) ? "No" : "Yes")
                  << std::endl;

        if (glyph.bitmap.pixels != nullptr) {
            showBitmap(glyph.bitmap);
        }
    }
}

auto IBMFFaceLow::showGlyphInfo(GlyphCode i, const GlyphInfo &g) const -> void {
    if constexpr (DEBUG) {
        std::cout << "  [" << i << "]: w: " << +g.bitmapWidth << ", h: " << +g.bitmapHeight
                  << ", hoff: " << +g.horizontalOffset << ", voff: " << +g.verticalOffset
                  << ", pktLen: " << +g.packetLength
                  << ", adv: " << +(static_cast<float>(g.advance) / 64.0)
                  << ", dynF: " << +g.rleMetrics.dynF
                  << ", 1stBlack: " << +g.rleMetrics.firstIsBlack
                  << ", lKPgmIdx: " << +g.ligKernPgmIndex
                  << ", poolIdx: " << +(*glyphsPixelPoolIndexes_)[i];
        if (g.mainCode != i) {
            std::cout << ", mainCode: " << g.mainCode;
        }
        std::cout << std::endl;
    }
}

auto IBMFFaceLow::showLigKerns() const -> void {
    if constexpr (DEBUG) {
        std::cout << std::endl << "----------- Ligature / Kern programs: ----------" << std::endl;
        uint16_t i;
        for (i = 0; i < faceHeader_->ligKernStepCount; i++) {
            LigKernStep *entry = &(*ligKernSteps_)[i];
            if (entry->b.goTo.isAKern && entry->b.goTo.isAGoTo) {
                std::cout << "  [" << i << "]:  "
                          << "Goto: " << entry->b.goTo.displacement;
            } else {
                std::cout << "  [" << i << "]:  "
                          << "Stop: " << (entry->a.stop ? "Yes" : "No") << ", "
                          << "NxtGlyphCode: " << +entry->a.nextGlyphCode << ", "
                          << "IsKern: " << (entry->b.kern.isAKern ? "Yes" : "No") << ", "
                          << (entry->b.kern.isAKern ? "Kern Value: " : "Lig char: ");
                if (entry->b.kern.isAKern) {
                    std::cout << static_cast<float>(entry->b.kern.kerningValue / 64.0);
                } else {
                    std::cout << entry->b.repl.replGlyphCode;
                }
            }

            std::cout << std::endl;
        }
    }
}

auto IBMFFaceLow::showFace() const -> void {
    if constexpr (DEBUG) {

        std::cout << std::endl << "----------- Face Header: ----------" << std::endl;

        std::cout << "DPI: " << faceHeader_->dpi << ", point siz: " << +faceHeader_->pointSize
                  << ", linHght: " << +faceHeader_->lineHeight
                  << ", xHght: " << +(static_cast<float>(faceHeader_->xHeight) / 64.0)
                  << ", emSiz: " << +(static_cast<float>(faceHeader_->emHeight) / 64.0)
                  << ", spcSiz: " << +faceHeader_->spaceSize
                  << ", glyphCnt: " << +faceHeader_->glyphCount
                  << ", LKCnt: " << +faceHeader_->ligKernStepCount
                  << ", PixPoolSiz: " << +faceHeader_->pixelsPoolSize
                  << ", slantCorr: " << +(static_cast<float>(faceHeader_->slantCorrection) / 64.0)
                  << ", descHght: " << +faceHeader_->descenderHeight << std::endl;

        std::cout << std::endl << "----------- Glyphs: ----------" << std::endl;

        for (int i = 0; i < faceHeader_->glyphCount; i++) {
            showGlyphInfo(i, (*glyphsInfo_)[i]);
        }

        showLigKerns();
    }
}