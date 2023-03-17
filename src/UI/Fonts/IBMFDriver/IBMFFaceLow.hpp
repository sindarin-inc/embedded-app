#pragma once

#include <cstring>

#include "IBMFDefs.hpp"
#include "RLEExtractor.hpp"

using namespace IBMFDefs;

/**
 * @brief Access to a IBMF face.
 *
 * This is a low-level class to allow acces to a IBMF faces generated through METAFONT
 *
 */
class IBMFFaceLow {

private:
    static constexpr char const *TAG = "IBMFFaceLow";

    bool initialized_;
    FontFormat fontFormat_;
    PixelResolution resolution_;

    FaceHeaderPtr faceHeader_;
    PlanesPtr planes_;
    CodePointBundlesPtr codePointBundles_;

    RLEBitmap bitmaps_[MAX_GLYPH_COUNT];
    GlyphInfoPtr glyphs_[MAX_GLYPH_COUNT];
    LigKernStep (*ligKernSteps_)[];

public:
    IBMFFaceLow()
        : initialized_(false), fontFormat_(FontFormat::UNKNOWN), resolution_(DEFAULT_RESOLUTION),
          faceHeader_(nullptr), planes_(nullptr), codePointBundles_(nullptr) {}

    ~IBMFFaceLow() {}

    inline static auto fromFIX16(FIX16 val) -> float { return (float)val / 64.0; }
    inline static auto toFIX16(float val) -> FIX16 { return (FIX16)(val * 64.0); }
    inline static auto toFIX16(FIX14 val) -> FIX16 {
        return (FIX16)(((val & 0x4000) == 0) ? val : (val | 0xC000));
    }
    inline static auto fromFIX14(FIX14 val) -> float { return (float)toFIX16(val) / 64.0; }

    // ---

    auto load(const MemoryPtr dataStart, const int dataLength, FontFormat fontFmt) -> bool {
        memset(glyphs_, 0, sizeof(glyphs_));

        MemoryPtr memoryPtr = dataStart;
        MemoryPtr memoryEnd = dataStart + dataLength;

        faceHeader_ = (FaceHeaderPtr)memoryPtr;
        fontFormat_ = fontFmt;

        if (faceHeader_->glyphCount >= MAX_GLYPH_COUNT) {
            LOGE("Too many glyphs in face. Maximum is %d in IBMFDefs.h", MAX_GLYPH_COUNT);
            return false;
        }
        memoryPtr += sizeof(FaceHeader);
        for (int i = 0; i < faceHeader_->glyphCount; i++) {
            glyphs_[i] = (GlyphInfoPtr)memoryPtr;
            memoryPtr += sizeof(GlyphInfo);
            if (memoryPtr > memoryEnd) {
                LOGE("IBMFFace: End of memory (0x%p, 0x%p, %d) reached reading glyph #%d!!",
                     dataStart, memoryEnd, dataLength, i);
                return false;
            }

            bitmaps_[i] = RLEBitmap{.pixels = memoryPtr,
                                    .dim = Dim(glyphs_[i]->bitmapWidth, glyphs_[i]->bitmapHeight),
                                    .length = glyphs_[i]->packetLength};
            memoryPtr += glyphs_[i]->packetLength;

            if (memoryPtr > memoryEnd) {
                LOGE("IBMFFace: End of memory (0x%p, 0x%p, %d) reached reading bitmap #%d of "
                     "length %d!!",
                     dataStart, memoryEnd, dataLength, i, glyphs_[i]->packetLength);
                return false;
            }
        }

        ligKernSteps_ = (LigKernStep(*)[])memoryPtr;
        memoryPtr += sizeof(LigKernStep) * faceHeader_->ligKernStepCount;
        if (memoryPtr > memoryEnd) {
            LOGE("IBMFFace: End of memory reached reading lig/kern struct!!");
            return false;
        }

        // showFace();
        initialized_ = true;
        return true;
    }

    inline auto isInitialized() const -> bool { return initialized_; }
    inline auto getFacePtSize() const -> uint8_t { return faceHeader_->pointSize; }
    inline auto getLineHeight() const -> uint16_t { return faceHeader_->lineHeight; }
    inline auto getEmHeight() const -> uint16_t { return faceHeader_->emHeight >> 6; }
    inline auto getMaxHight() const -> uint8_t {
        return (faceHeader_ != nullptr) ? faceHeader_->maxHight : 0;
    }
    inline auto getDescenderHeight() const -> int16_t {
        return -(int16_t)faceHeader_->descenderHeight;
    }
    inline auto getLigKernStep(uint16_t idx) const -> LigKernStepPtr {
        return &(*ligKernSteps_)[idx];
    }
    inline auto setResolution(PixelResolution res) -> void { resolution_ = res; }
    inline auto getResolution() const -> PixelResolution { return resolution_; }

    inline auto getGlyphInfo(GlyphCode glyphCode) const -> const GlyphInfo & {
        return *glyphs_[glyphCode];
    }

    inline auto getLigKernPgmIndex(GlyphCode glyphCode) const -> uint16_t {
        // LOGD("glyphCode: %d", glyphCode & GLYPH_CODE_MASK);
        return ((glyphCode & GLYPH_CODE_MASK) < SPACE_CODE)
                   ? glyphs_[glyphCode & GLYPH_CODE_MASK]->ligKernPgmIndex
                   : NO_LIG_KERN_PGM;
    }

    /**
     * @brief Translate UTF32 charCode to it's internal représentation
     *
     * All supported character sets must use this method to get the internal
     * glyph code correponding to the character code
     *
     * Latin Char Set Translation
     *
     * The class allow for latin1+ characters to be plotted into a bitmap. As the
     * font doesn't contains all accented glyphs, a combination of glyphs must be
     * used to draw a single bitmap. This method translate some of the supported
     * unicode values to that combination. The least significant byte will contains
     * the main glyph code and the next byte will contain the accent code.
     *
     * The supported UTF16 character codes supported are the following:
     *
     *      U+0020 to U+007F
     *      U+00A1 to U+017f
     *
     *   and the following:
     *
     * |  UTF16  | Description          |
     * |:-------:|----------------------|
     * | U+02BB  | reverse apostrophe
     * | U+02BC  | apostrophe
     * | U+02C6  | circumflex
     * | U+02DA  | ring
     * | U+02DC  | tilde ~
     * | U+2013  | endash (Not available with CM Typewriter)
     * | U+2014  | emdash (Not available with CM Typewriter)
     * | U+2018  | quote left
     * | U+2019  | quote right
     * | U+201A  | comma like ,
     * | U+201C  | quoted left "
     * | U+201D  | quoted right
     * | U+2032  | minute '
     * | U+2033  | second "
     * | U+2044  | fraction /
     * | U+20AC  | euro
     *
     * @param charCode The UTF16 character code
     * @return The internal representation of a character (glyphCode)
     */
    auto translate(char32_t charCode) const -> GlyphCode {
        GlyphCode glyphCode = SPACE_CODE;

        if (fontFormat_ == FontFormat::LATIN) {
            if ((charCode > 0x20) && (charCode < 0x7F)) {
                glyphCode = charCode; // ASCII codes No accent
            } else if ((charCode >= 0xA1) && (charCode <= 0x1FF)) {
                glyphCode = latinTranslationSet[charCode - 0xA1];
            } else {
                switch (charCode) {
                case 0x2013: // endash
                    glyphCode = 0x0015;
                    break;
                case 0x2014: // emdash
                    glyphCode = 0x0016;
                    break;
                case 0x2018: // quote left
                case 0x02BB: // reverse apostrophe
                    glyphCode = 0x0060;
                    break;
                case 0x2019: // quote right
                case 0x02BC: // apostrophe
                    glyphCode = 0x0027;
                    break;
                case 0x201C: // quoted left "
                    glyphCode = 0x0010;
                    break;
                case 0x201D: // quoted right
                    glyphCode = 0x0011;
                    break;
                case 0x02C6: // circumflex
                    glyphCode = 0x005E;
                    break;
                case 0x02DA: // ring
                    glyphCode = 0x0006;
                    break;
                case 0x02DC: // tilde ~
                    glyphCode = 0x007E;
                    break;
                case 0x201A: // comma like ,
                    glyphCode = 0x000D;
                    break;
                case 0x2032: // minute '
                    glyphCode = 0x0027;
                    break;
                case 0x2033: // second "
                    glyphCode = 0x0022;
                    break;
                case 0x2044: // fraction /
                    glyphCode = 0x002F;
                    break;
                case 0x20AC: // euro
                    glyphCode = 0x00AD;
                    break;
                }
            }
        } else if (fontFormat_ == FontFormat::UTF32) {
            uint16_t planeIdx = static_cast<uint16_t>(charCode >> 16);

            if (planeIdx <= 3) {
                char16_t u16 = static_cast<char16_t>(charCode);

                uint16_t codePointBundleIdx = (*planes_)[planeIdx].codePointBundlesIdx;
                uint16_t entriesCount = (*planes_)[planeIdx].entriesCount;
                int gCode = (*planes_)[planeIdx].firstGlyphCode;
                int i = 0;
                while (i < entriesCount) {
                    if (u16 < (*codePointBundles_)[codePointBundleIdx].endCodePoint) {
                        break;
                    }
                    gCode += ((*codePointBundles_)[codePointBundleIdx].endCodePoint -
                              (*codePointBundles_)[codePointBundleIdx].firstCodePoint);
                    i++;
                    codePointBundleIdx++;
                }
                if ((i < entriesCount) &&
                    (u16 >= (*codePointBundles_)[codePointBundleIdx].firstCodePoint)) {
                    glyphCode =
                        gCode + u16 - (*codePointBundles_)[codePointBundleIdx].firstCodePoint;
                }
            }
        }

        return glyphCode;
    }

    auto getGlyph(GlyphCode glyphCode, Glyph &appGlyph, bool loadBitmap, bool caching = true,
                  Pos atPos = Pos(0, 0)) -> bool {

        // LOGD("glyphCode: %04x, loadBitmap: %s, caching: %s, pos: [%d, %d]", glyphCode,
        //      loadBitmap ? "YES" : "NO", caching ? "YES" : "NO", atPos.x, atPos.y);

        bool accentIsPresent = false;
        uint8_t accentIdx = 0;
        GlyphInfoPtr accentInfo = nullptr;

        if (caching) {
            appGlyph.clear();
        }

        if (fontFormat_ == FontFormat::LATIN) {
            accentIsPresent = (glyphCode & ACCENT_MASK) != 0;
            if (accentIsPresent) {
                accentIdx = (glyphCode & ACCENT_MASK) >> ACCENT_SHIFTR;
                if (accentIdx == CODED_GRAVE_ACCENT) {
                    accentIdx = GRAVE_ACCENT;
                } else if (accentIdx == CODED_APOSTROPHE) {
                    accentIdx = APOSTROPHE;
                }
                accentInfo = glyphs_[accentIdx];
            }
        } else if (fontFormat_ == FontFormat::UTF32) {
            // Next release of the IBMF file format will have a table to support
            // direct usage of predefined UTF-16 characters. Up to 1022 characters
            // will be possible, unless extended to use more space in memory.
            //
            // It will be known as UTF32
            LOGW("Support for UTF32 not ready.");
            return false;
        } else {
            LOGE("Unknown character set: %d", fontFormat_);
        }

        uint8_t glyphIdx = (glyphCode & GLYPH_CODE_MASK) - faceHeader_->firstCode;
        if (((glyphCode & GLYPH_CODE_MASK) == NO_GLYPH_CODE) ||
            ((glyphIdx < faceHeader_->glyphCount) && (glyphs_[glyphIdx] == nullptr))) {
            glyphCode = SPACE_CODE;
        }

        if (glyphCode == SPACE_CODE) { // send as a space character
            appGlyph.metrics.lineHeight = faceHeader_->lineHeight;
            appGlyph.metrics.advance = (int16_t)faceHeader_->spaceSize << 6;
            appGlyph.pointSize = faceHeader_->pointSize;
            return true;
        }

        if (glyphIdx >= faceHeader_->glyphCount) {
            return false;
        }

        GlyphInfoPtr glyphInfo = glyphs_[glyphIdx];

        if (glyphInfo == nullptr) {
            return false;
        }

        Dim dim = Dim(glyphInfo->bitmapWidth, glyphInfo->bitmapHeight);
        Pos glyphOffsets = Pos(0, 0);
        Pos accentOffsets = Pos(0, 0);

        if (accentIsPresent) {

            if (glyphCode == 0xE06E) { // Apostrophe n
                // offsets.x = 0; // already set
                glyphOffsets.x =
                    accentInfo->bitmapWidth + 1 -
                    (((faceHeader_->xHeight >> 6) * faceHeader_->slantCorrection) >> 6);
                dim.width = glyphOffsets.x + glyphInfo->bitmapWidth;
            } else {
                // Horizontal adjustment
                if (glyphCode == 0xC041) { // Ą
                    accentOffsets.x = glyphInfo->bitmapWidth - accentInfo->bitmapWidth;
                } else if ((glyphCode == 0xC061) || (glyphCode == 0xC045)) { // ą or Ę
                    accentOffsets.x =
                        glyphInfo->bitmapWidth - accentInfo->bitmapWidth -
                        ((((int32_t)glyphInfo->bitmapHeight) * faceHeader_->slantCorrection) >> 6);
                } else {
                    accentOffsets.x =
                        ((glyphInfo->bitmapWidth > accentInfo->bitmapWidth)
                             ? ((glyphInfo->bitmapWidth - accentInfo->bitmapWidth) >> 1)
                             : 0) +
                        ((accentInfo->verticalOffset < 5)
                             ? -((faceHeader_->xHeight * faceHeader_->slantCorrection) >> 6)
                             : ((((int32_t)glyphInfo->bitmapHeight) *
                                 faceHeader_->slantCorrection) >>
                                6));
                    /*- (accentInfo->horizontalOffset - glyphInfo->horizontalOffset)*/;
                }
                if ((accentOffsets.x == 0) && (glyphInfo->bitmapWidth < accentInfo->bitmapWidth)) {
                    glyphOffsets.x = (accentInfo->bitmapWidth - glyphInfo->bitmapWidth) >> 1;
                    dim.width = accentInfo->bitmapWidth;
                }
                if (dim.width < (accentOffsets.x + accentInfo->bitmapWidth)) {
                    dim.width = accentOffsets.x + accentInfo->bitmapWidth;
                }
            }

            // Vertical adjustment
            if (caching) {
                if (accentInfo->verticalOffset >= (faceHeader_->xHeight >> 6)) {
                    // Accents that are on top of a main glyph
                    glyphOffsets.y = accentInfo->verticalOffset - (faceHeader_->xHeight >> 6);
                    dim.height += glyphOffsets.y;
                } else if (accentInfo->verticalOffset < 5) {
                    // Accents below the main glyph (cedilla)
                    int16_t addedHeightBelow =
                        (glyphInfo->bitmapHeight - glyphInfo->verticalOffset) -
                        ((-accentInfo->verticalOffset) + accentInfo->bitmapHeight);
                    if (addedHeightBelow < 0) {
                        dim.height += -addedHeightBelow;
                    }
                    accentOffsets.y = glyphInfo->verticalOffset - accentInfo->verticalOffset;
                }
            } else {
                if (accentInfo->verticalOffset >= (faceHeader_->xHeight >> 6)) {
                    accentOffsets.y = -(accentInfo->verticalOffset - (faceHeader_->xHeight >> 6) +
                                        glyphInfo->verticalOffset);
                    glyphOffsets.y = -glyphInfo->verticalOffset;
                } else if (accentInfo->verticalOffset < 5) {
                    glyphOffsets.y = -glyphInfo->verticalOffset;
                }
            }
            // else if ((glyphCode == 0x1648) || (glyphCode == 0x1568)) { // Ħ or ħ
            //   offsets.y = (glyphInfo->bitmapHeight * 1) >> 2;
            //   offsets.x = ((glyphInfo->bitmapHeight - offsets.y) *
            //   faceHeader_->slantCorrection) >> 6;
            // }
            // else if ((glyphCode == 0x1554) || (glyphCode == 0x1574)) { // Ŧ or ŧ
            //   offsets.y = (glyphInfo->bitmapHeight * 1) >> 1;
            //   offsets.x = (offsets.y * faceHeader_->slantCorrection) >> 6;
            // }
        } else {
            glyphOffsets.y = -glyphInfo->verticalOffset;
        }

        if (!caching) {
            glyphOffsets.x -= glyphInfo->horizontalOffset;
            accentOffsets.x -= glyphInfo->horizontalOffset;
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

                appGlyph.bitmap.pixels = new uint8_t[size];
                memset(appGlyph.bitmap.pixels, white, size);
                appGlyph.bitmap.dim = dim;
            }

            RLEBitmapPtr glyphBitmap = &bitmaps_[glyphIdx];
            if (glyphBitmap != nullptr) {
                RLEExtractor rle(resolution_);

                Pos outPos = Pos(atPos.x + accentOffsets.x, atPos.y + accentOffsets.y);
                if (accentIsPresent) {
                    RLEBitmapPtr accentBitmap = &bitmaps_[accentIdx];
                    rle.retrieveBitmap(*accentBitmap, appGlyph.bitmap, outPos,
                                       accentInfo->rleMetrics);

                    showBitmap(appGlyph.bitmap);
                }

                outPos = Pos(atPos.x + glyphOffsets.x, atPos.y + glyphOffsets.y);
                rle.retrieveBitmap(*glyphBitmap, appGlyph.bitmap, outPos, glyphInfo->rleMetrics);
            }
        }

        appGlyph.metrics = {
            .xoff = (int16_t) - (glyphOffsets.x + ((caching) ? glyphInfo->horizontalOffset : 0)),
            .yoff = (int16_t) - (glyphOffsets.y + ((caching) ? glyphInfo->verticalOffset : 0)),
            .advance = glyphInfo->advance,
            .lineHeight = faceHeader_->lineHeight,
            .ligatureAndKernPgmIndex = glyphInfo->ligKernPgmIndex};

        // showGlyph(appGlyph, glyphCode & 0xFF);
        return true;
    }

    auto showBitmap(const Bitmap &bitmap) const -> void {
        if constexpr (DEBUG) {
            uint32_t row, col;
            MemoryPtr rowPtr;

            std::cout << "   +";
            for (col = 0; col < bitmap.dim.width; col++)
                std::cout << '-';
            std::cout << '+' << std::endl;

            if (resolution_ == PixelResolution::ONE_BIT) {
                uint32_t rowSize = (bitmap.dim.width + 7) >> 3;
                for (row = 0, rowPtr = bitmap.pixels; row < bitmap.dim.height;
                     row++, rowPtr += rowSize) {
                    std::cout << "   |";
                    for (col = 0; col < bitmap.dim.width; col++) {
                        if constexpr (BLACK_ONE_BIT) {
                            std::cout << ((rowPtr[col >> 3] & (0x80 >> (col & 7))) ? 'X' : ' ');
                        } else {
                            std::cout << ((rowPtr[col >> 3] & (0x80 >> (col & 7))) ? ' ' : 'X');
                        }
                    }
                    std::cout << '|';
                    std::cout << std::endl;
                }
            } else {
                uint32_t rowSize = bitmap.dim.width;
                for (row = 0, rowPtr = bitmap.pixels; row < bitmap.dim.height;
                     row++, rowPtr += rowSize) {
                    std::cout << "   |";
                    for (col = 0; col < bitmap.dim.width; col++) {
                        if constexpr (BLACK_EIGHT_BITS) {
                            std::cout << ((rowPtr[col] == BLACK_EIGHT_BITS) ? 'X' : ' ');
                        } else {
                            std::cout << ((rowPtr[col] == BLACK_EIGHT_BITS) ? 'X' : ' ');
                        }
                    }
                    std::cout << '|';
                    std::cout << std::endl;
                }
            }

            std::cout << "   +";
            for (col = 0; col < bitmap.dim.width; col++) {
                std::cout << '-';
            }
            std::cout << '+' << std::endl << std::endl;
        }
    }

    auto showGlyph(const Glyph &glyph, GlyphCode glyphCode, char16_t charCode = ' ') const -> void {
        if constexpr (DEBUG) {
            std::wcout << "Glyph Base Code: 0x" << std::hex << glyphCode << std::dec << "("
                       << charCode << ")" << std::endl
                       << "  Metrics: [" << std::dec << glyph.bitmap.dim.width << ", "
                       << glyph.bitmap.dim.height << "] " << std::endl
                       << "  Position: [" << glyph.metrics.xoff << ", " << glyph.metrics.yoff << ']'
                       << std::endl
                       << "  Bitmap available: "
                       << ((glyph.bitmap.pixels == nullptr) ? "No" : "Yes") << std::endl;

            if (glyph.bitmap.pixels != nullptr) {
                showBitmap(glyph.bitmap);
            }
        }
    }

    auto showGlyph2(const Glyph &glyph, char16_t charCode) const -> void {
        showGlyph(glyph, 0, charCode);
    }

    auto showGlyphInfo(GlyphCode i, GlyphInfo &g) const -> void {
        if constexpr (DEBUG) {
            std::cout << "  [" << i << "]: "
                      << "charCode : " << +g.charCode << ", width: " << +g.bitmapWidth
                      << ", height: " << +g.bitmapHeight << ", hoffset: " << +g.horizontalOffset
                      << ", voffset: " << +g.verticalOffset << ", pktLen: " << +g.packetLength
                      << ", advance: " << +((float)g.advance / 64.0)
                      << ", dynF: " << +g.rleMetrics.dynF
                      << ", firstIsBlack: " << +g.rleMetrics.firstIsBlack
                      << ", ligKernPgmIndex: " << +g.ligKernPgmIndex << std::endl;
        }
    }

    auto showLigKerns() const -> void {
        if constexpr (DEBUG) {
            std::cout << std::endl
                      << "----------- Ligature / Kern programs: ----------" << std::endl;
            uint16_t i;
            for (i = 0; i < faceHeader_->ligKernStepCount; i++) {
                LigKernStepPtr entry = &(*ligKernSteps_)[i];
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
                        std::cout << (float)(entry->b.kern.kerningValue / 64.0);
                    } else {
                        std::cout << entry->b.repl.replGlyphCode;
                    }
                }

                std::cout << std::endl;
            }
        }
    }

    auto showFace() const -> void {
        if constexpr (DEBUG) {

            std::cout << std::endl << "----------- Header: ----------" << std::endl;

            std::cout << "DPI: " << faceHeader_->dpi << ", point size: " << +faceHeader_->pointSize
                      << ", first glyph code: " << +faceHeader_->firstCode
                      << ", last glyph code: " << +faceHeader_->lastCode
                      << ", line height: " << +faceHeader_->lineHeight
                      << ", max height: " << +faceHeader_->maxHight
                      << ", x height: " << +((float)faceHeader_->xHeight / 64.0)
                      << ", em size: " << +((float)faceHeader_->emHeight / 64.0)
                      << ", space size: " << +((float)faceHeader_->spaceSize / 64.0)
                      << ", glyph count: " << +faceHeader_->glyphCount
                      << ", lig kern count: " << +faceHeader_->ligKernStepCount
                      << ", slant corr: " << +faceHeader_->slantCorrection
                      << ", descender height: " << +faceHeader_->descenderHeight << std::endl;

            std::cout << std::endl << "----------- Glyphs: ----------" << std::endl;

            for (int i = 0; i < faceHeader_->glyphCount; i++) {
                showGlyphInfo(i, *glyphs_[i]);
            }

            showLigKerns();
        }
    }
};
