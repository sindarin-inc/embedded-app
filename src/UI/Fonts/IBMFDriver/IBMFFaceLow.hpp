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

    static constexpr uint8_t MAX_GLYPH_COUNT = 254; // Index Value 0xFE and 0xFF are reserved

    bool initialized;

    FaceHeaderPtr faceHeader;
    RLEBitmap bitmaps[MAX_GLYPH_COUNT];
    GlyphInfoPtr glyphs[MAX_GLYPH_COUNT];
    LigKernStep *ligKernSteps;
    FIX16 *kerns;

public:
    IBMFFaceLow() : initialized(false) {}

    ~IBMFFaceLow() {}

    bool load(const MemoryPtr dataStart, const int dataLength) {
        memset(glyphs, 0, sizeof(glyphs));

        MemoryPtr memoryPtr = dataStart;
        MemoryPtr memoryEnd = dataStart + dataLength;

        faceHeader = (FaceHeaderPtr)memoryPtr;

        if (faceHeader->glyphCount >= MAX_GLYPH_COUNT) {
            LOGE("Too many glyphs in face. Maximum is %d in IBMFDefs.h", MAX_GLYPH_COUNT);
            return false;
        }
        memoryPtr += sizeof(FaceHeader);
        for (int i = 0; i < faceHeader->glyphCount; i++) {
            glyphs[i] = (GlyphInfoPtr)memoryPtr;
            memoryPtr += sizeof(GlyphInfo);
            if (memoryPtr > memoryEnd) {
                LOGE("IBMFFace: End of memory (0x%p, 0x%p, %d) reached reading glyph #%d!!",
                     dataStart, memoryEnd, dataLength, i);
                return false;
            }

            bitmaps[i] = RLEBitmap{
                pixels : memoryPtr,
                dim : Dim(glyphs[i]->bitmapWidth, glyphs[i]->bitmapHeight),
                length : glyphs[i]->packetLength
            };
            memoryPtr += glyphs[i]->packetLength;

            if (memoryPtr > memoryEnd) {
                LOGE("IBMFFace: End of memory (0x%p, 0x%p, %d) reached reading bitmap #%d of "
                     "length %d!!",
                     dataStart, memoryEnd, dataLength, i, glyphs[i]->packetLength);
                return false;
            }
        }

        ligKernSteps = (LigKernStep *)memoryPtr;
        memoryPtr += sizeof(LigKernStep) * faceHeader->ligKernStepCount;
        if (memoryPtr > memoryEnd) {
            LOGE("IBMFFace: End of memory reached reading lig/kern struct!!");
            return false;
        }

        kerns = (FIX16 *)memoryPtr;

        memoryPtr += sizeof(FIX16) * faceHeader->kernCount;
        if (memoryPtr > memoryEnd) {
            LOGE("IBMFFace: End of memory reached reading kerns!!");
            return false;
        }

        showFace();
        initialized = true;
        return true;
    }

    inline bool isInitialized() const { return initialized; }
    inline uint8_t getFacePtSize() const { return faceHeader->pointSize; }
    inline uint16_t getLineHeight() const { return faceHeader->lineHeight; }
    inline uint16_t getEmHeight() const { return faceHeader->emHeight >> 6; }
    inline uint8_t getMaxHight() const { return faceHeader->maxHight; }
    inline int16_t getDescenderHeight() const { return -(int16_t)faceHeader->descenderHeight; }
    inline const LigKernStep *getLigKern(uint8_t idx) const { return &ligKernSteps[idx]; }
    inline FIX16 getKern(uint8_t i) const { return kerns[i]; }
    inline const GlyphInfo *getGlyphInfo(uint8_t glyphCode) const { return glyphs[glyphCode]; }

    /**
     * @brief Translate unicode in an internal char code
     *
     * The class allow for latin1+ characters to be plotted into a bitmap. As the
     * font doesn't contains all accented glyphs, a combination of glyphs must be
     * used to draw a single bitmap. This method translate some of the supported
     * unicode values to that combination. The least significant byte will contains
     * the main glyph code and the next byte will contain the accent code.
     *
     * @param charcode The character code in unicode
     * @return The internal representation of a character
     */
    uint32_t translate(uint32_t charcode) const {
        uint32_t glyphCode;

        if ((charcode > 0x20) && (charcode < 0x7F)) {
            glyphCode = 0xFF00 | charcode; // ASCII codes No accent
        } else if ((charcode >= 0xA1) && (charcode <= 0xFF)) {
            glyphCode = set2TranslationLatin1[charcode - 0xA1];
        } else if ((charcode >= 0x100) && (charcode <= 0x1FF)) {
            glyphCode = set2TranslationLatinA[charcode - 0x100];
        } else {
            switch (charcode) {
            case 0x2013:
                glyphCode = 0xFF15;
                break; // endash
            case 0x2014:
                glyphCode = 0xFF16;
                break;   // emdash
            case 0x2018: // quote left
            case 0x02BB: // reverse apostrophe
                glyphCode = 0xFF60;
                break;
            case 0x2019: // quote right
            case 0x02BC: // apostrophe
                glyphCode = 0xFF27;
                break;
            case 0x201C:
                glyphCode = 0xFF10;
                break; // quoted left "
            case 0x201D:
                glyphCode = 0xFF11;
                break; // quoted right
            case 0x02C6:
                glyphCode = 0xFF5E;
                break; // circumflex
            case 0x02DA:
                glyphCode = 0xFF06;
                break; // ring
            case 0x02DC:
                glyphCode = 0xFF7E;
                break; // tilde ~
            case 0x201A:
                glyphCode = 0xFF0D;
                break; // comma like ,
            case 0x2032:
                glyphCode = 0xFF27;
                break; // minute '
            case 0x2033:
                glyphCode = 0xFF22;
                break; // second "
            case 0x2044:
                glyphCode = 0xFF2F;
                break; // fraction /
            case 0x20AC:
                glyphCode = 0xFFAD;
                break; // euro
            default:
                glyphCode = 0xFFFE;
            }
        }

        return glyphCode;
    }

    bool getGlyph(uint32_t charCode, Glyph &appGlyph, bool loadBitmap, bool bitmapInit = true,
                  Pos atPos = Pos(0, 0)) {

        uint32_t glyphCode = translate(charCode);

        if (bitmapInit) appGlyph.clear();
        appGlyph.metrics.lineHeight = faceHeader->lineHeight;

        uint8_t accentCode = (glyphCode & 0x0000FF00) >> 8;
        uint8_t accentIdx = accentCode - faceHeader->firstCode;
        GlyphInfoPtr accentInfo = (accentCode != 0xFF) ? glyphs[accentIdx] : nullptr;

        uint8_t glyphIdx = (glyphCode & 0xFF) - faceHeader->firstCode;
        if (((glyphCode & 0xFF) == 0xFF) ||
            ((glyphIdx < faceHeader->glyphCount) && (glyphs[glyphIdx] == nullptr))) {
            LOGW("No entry for char code %d (0x%x) [glyph code %d (0x%x)]", charCode, charCode,
                 glyphCode, glyphCode);
            glyphCode = 0xFFFE;
        }

        if (glyphCode == 0xFFFE) { // send as a space character
            appGlyph.metrics.advance = faceHeader->spaceSize;
            appGlyph.pointSize = faceHeader->pointSize;
            return true;
        }

        if (glyphIdx >= faceHeader->glyphCount) return false;

        GlyphInfoPtr glyphInfo = glyphs[glyphIdx];

        if (glyphInfo == nullptr) return false;

        Dim dim = Dim(glyphInfo->bitmapWidth, glyphInfo->bitmapHeight);
        Pos offsets = Pos(0, 0);

        uint8_t addedLeft = 0;

        if (accentInfo != nullptr) {

            if (glyphCode == 0x276E) { // Apostrophe n
                // offsets.x = 0; // already set
                addedLeft = accentInfo->bitmapWidth + 1 -
                            (((faceHeader->xHeight >> 6) * faceHeader->slantCorrection) >> 6);
                dim.width = addedLeft + glyphInfo->bitmapWidth;
            } else {
                // Horizontal adjustment
                if (glyphCode == 0x0C41) { // Ą
                    offsets.x = glyphInfo->bitmapWidth - accentInfo->bitmapWidth;
                } else if ((glyphCode == 0x0C61) || (glyphCode == 0x0C45)) { // ą or Ę
                    offsets.x =
                        glyphInfo->bitmapWidth - accentInfo->bitmapWidth -
                        ((((int32_t)glyphInfo->bitmapHeight) * faceHeader->slantCorrection) >> 6);
                } else {
                    offsets.x =
                        ((glyphInfo->bitmapWidth > accentInfo->bitmapWidth)
                             ? ((glyphInfo->bitmapWidth - accentInfo->bitmapWidth) >> 1)
                             : 0) +
                        ((accentInfo->verticalOffset < 5)
                             ? -(((faceHeader->xHeight >> 6) * faceHeader->slantCorrection) >> 6)
                             : ((((int32_t)glyphInfo->bitmapHeight) *
                                 faceHeader->slantCorrection) >>
                                6))
                        /*- (accentInfo->horizontalOffset - glyphInfo->horizontalOffset)*/;
                }
                if ((offsets.x == 0) && (glyphInfo->bitmapWidth < accentInfo->bitmapWidth)) {
                    addedLeft = (accentInfo->bitmapWidth - glyphInfo->bitmapWidth) >> 1;
                    dim.width = accentInfo->bitmapWidth;
                }
                if (dim.width < (offsets.x + accentInfo->bitmapWidth)) {
                    dim.width = offsets.x + accentInfo->bitmapWidth;
                }
            }

            // Vertical adjustment
            if (accentInfo->verticalOffset >= (faceHeader->xHeight >> 6)) {
                // Accents that are on top of a main glyph
                dim.height += (accentInfo->verticalOffset - (faceHeader->xHeight >> 6));
            } else if (accentInfo->verticalOffset < 5) {
                // Accents below the main glyph (cedilla)
                int16_t addedHeight = (glyphInfo->bitmapHeight - glyphInfo->verticalOffset) -
                                      ((-accentInfo->verticalOffset) + accentInfo->bitmapHeight);
                if (addedHeight < 0) dim.height += -addedHeight;
                offsets.y = glyphInfo->verticalOffset - accentInfo->verticalOffset;
            }
            // else if ((glyphCode == 0x1648) || (glyphCode == 0x1568)) { // Ħ or ħ
            //   offsets.y = (glyphInfo->bitmapHeight * 1) >> 2;
            //   offsets.x = ((glyphInfo->bitmapHeight - offsets.y) *
            //   faceHeader->slantCorrection) >> 6;
            // }
            // else if ((glyphCode == 0x1554) || (glyphCode == 0x1574)) { // Ŧ or ŧ
            //   offsets.y = (glyphInfo->bitmapHeight * 1) >> 1;
            //   offsets.x = (offsets.y * faceHeader->slantCorrection) >> 6;
            // }
        }

        if (loadBitmap) {
            if (bitmapInit) {
                uint16_t size = (resolution == PixelResolution::ONE_BIT)
                                    ? dim.height * ((dim.width + 7) >> 3)
                                    : dim.height * dim.width;

                appGlyph.bitmap.pixels = new uint8_t[size];
                memset(appGlyph.bitmap.pixels, 0, size);
                appGlyph.bitmap.dim = dim;
            }

            RLEBitmapPtr glyphBitmap = &bitmaps[glyphIdx];
            if (glyphBitmap != nullptr) {
                RLEExtractor rle;

                Pos outPos = Pos(atPos.x + offsets.x, atPos.y - glyphInfo->verticalOffset);
                RLEBitmapPtr accentBitmap = (accentCode != 0xFF) ? &bitmaps[accentIdx] : nullptr;
                if (accentBitmap != nullptr) {
                    rle.retrieveBitmap(*accentBitmap, appGlyph.bitmap, outPos,
                                       accentInfo->rleMetrics);

                    offsets.y = (accentInfo->verticalOffset >= (faceHeader->xHeight >> 6))
                                    ? (accentInfo->verticalOffset - (faceHeader->xHeight >> 6))
                                    : 0;
                    offsets.x = addedLeft;
                }

                outPos = Pos(atPos.x + offsets.x, atPos.y - glyphInfo->verticalOffset);
                rle.retrieveBitmap(*glyphBitmap, appGlyph.bitmap, outPos, glyphInfo->rleMetrics);
            }
        }

        appGlyph.metrics.xoff = -(glyphInfo->horizontalOffset + offsets.x);
        appGlyph.metrics.yoff = -(glyphInfo->verticalOffset + offsets.y);
        appGlyph.metrics.advance = glyphInfo->advance >> 6;
        appGlyph.metrics.ligatureAndKernPgmIndex = glyphInfo->ligKernPgmIndex;
        appGlyph.metrics.lineHeight = faceHeader->lineHeight;

        showGlyph(appGlyph, glyphCode & 0xFF);
        return true;
    }

    void showBitmap(const Bitmap &bitmap) const {
        if constexpr (DEBUG) {
            uint32_t row, col;
            MemoryPtr rowPtr;

            std::cout << '+';
            for (col = 0; col < bitmap.dim.width; col++)
                std::cout << '-';
            std::cout << '+' << std::endl;

            if (resolution == PixelResolution::ONE_BIT) {
                uint32_t rowSize = (bitmap.dim.width + 7) >> 3;
                for (row = 0, rowPtr = bitmap.pixels; row < bitmap.dim.height;
                     row++, rowPtr += rowSize) {
                    std::cout << '|';
                    for (col = 0; col < bitmap.dim.width; col++) {
                        std::cout << ((rowPtr[col >> 3] & (0x80 >> (col & 7))) ? 'X' : ' ');
                    }
                    std::cout << '|';
                    std::cout << std::endl;
                }
            } else {
                uint32_t rowSize = bitmap.dim.width;
                for (row = 0, rowPtr = bitmap.pixels; row < bitmap.dim.height;
                     row++, rowPtr += rowSize) {
                    std::cout << '|';
                    for (col = 0; col < bitmap.dim.width; col++) {
                        std::cout << ((rowPtr[col] == 0) ? ' ' : 'X');
                    }
                    std::cout << '|';
                    std::cout << std::endl;
                }
            }

            std::cout << '+';
            for (col = 0; col < bitmap.dim.width; col++) {
                std::cout << '-';
            }
            std::cout << '+' << std::endl << std::endl;
        }
    }

    void showGlyph(const Glyph &glyph, uint32_t charCode) const {
        if constexpr (DEBUG) {
            std::cout << "Glyph Char Code: 0x" << std::hex << charCode << std::dec << std::endl
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

    void showGlyphInfo(uint16_t i, GlyphInfo &g) const {
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

    void showLigKerns() const {
        if constexpr (DEBUG) {
            std::cout << std::endl
                      << "----------- Ligature / Kern programs: ----------" << std::endl;
            LigKernStepPtr entry;
            uint16_t i;
            for (entry = ligKernSteps, i = 0; i < faceHeader->ligKernStepCount; entry++, i++) {
                if (entry->skip.whole > 128) {
                    std::cout << "  [" << i << "]:  "
                              << "Whole skip: " << +entry->skip.whole << ", "
                              << "Goto: "
                              << +((entry->opCode.d.displHigh << 8) + entry->remainder.displLow);
                } else {
                    std::cout << "  [" << i << "]:  "
                              << "Whole skip: " << +entry->skip.whole << ", "
                              << "Stop: " << (entry->skip.s.stop ? "Yes" : "No") << ", "
                              << "NxtChar: " << +entry->nextChar << ", "
                              << "IsKern: " << (entry->opCode.d.isAKern ? "Yes" : "No") << ", "
                              << (entry->opCode.d.isAKern ? "Kern Idx: " : "Lig char: ")
                              << (entry->opCode.d.isAKern ? +((entry->opCode.d.displHigh << 8) +
                                                              entry->remainder.displLow)
                                                          : +entry->remainder.replacementChar)
                              << std::dec;
                    if (!entry->opCode.d.isAKern) {
                        std::cout << ", OpCodes: a:" << +entry->opCode.op.aOp
                                  << ", b:" << +entry->opCode.op.bOp
                                  << ", c:" << +entry->opCode.op.cOp;
                    }
                }

                std::cout << std::endl;
            }
        }
    }

    void showKerns() const {
        if constexpr (DEBUG) {
            std::cout << std::endl << "----------- Kerns: ----------" << std::endl;

            for (int i = 0; i < faceHeader->kernCount; i++) {
                std::cout << "  [" << i << "]:  " << (kerns[i] / 64.0) << std::endl;
            }
        }
    }

    void showFace() const {
        if constexpr (DEBUG) {

            std::cout << std::endl << "----------- Header: ----------" << std::endl;

            std::cout << "DPI: " << faceHeader->dpi << ", point size: " << +faceHeader->pointSize
                      << ", first char code: " << +faceHeader->firstCode
                      << ", last char code: " << +faceHeader->lastCode
                      << ", line height: " << +faceHeader->lineHeight
                      << ", max height: " << +faceHeader->maxHight
                      << ", x height: " << +((float)faceHeader->xHeight / 64.0)
                      << ", em size: " << +((float)faceHeader->emHeight / 64.0)
                      << ", space size: " << +((float)faceHeader->spaceSize / 64.0)
                      << ", glyph count: " << +faceHeader->glyphCount
                      << ", lig kern count: " << +faceHeader->ligKernStepCount
                      << ", kernCount: " << +faceHeader->kernCount
                      << ", slant corr: " << +faceHeader->slantCorrection
                      << ", descender height: " << +faceHeader->descenderHeight << std::endl;

            std::cout << std::endl << "----------- Glyphs: ----------" << std::endl;

            for (int i = 0; i < faceHeader->glyphCount; i++) {
                showGlyphInfo(i, *glyphs[i]);
            }

            showLigKerns();

            showKerns();
        }
    }
};
