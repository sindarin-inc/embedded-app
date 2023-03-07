#pragma once

#include <cstdarg>
#include <cstdio>
#include <functional>

#include "IBMFFontLow.hpp"
#include "UI/Fonts/Font.h"
#include "UI/Fonts/IBMFDriver/UTF8Iterator.hpp"

class IBMFFont : public Font {
private:
    static constexpr char const *TAG = "IBMFFont";

    IBMFFontLow *font;
    IBMFFaceLow *face;

    // Maximum size of an allocated buffer to do vsnprintf formatting
    const int MAX_SIZE = 100;

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
    bool ligKern(const GlyphCode glyphCode1, GlyphCode *glyphCode2, FIX16 *kern) const {
        uint16_t lkIdx = face->getLigKernPgmIndex(glyphCode1);
        if (lkIdx == NO_LIG_KERN_PGM) return false;
        LigKernStepPtr lk = face->getLigKernStep(lkIdx);
        if (lk->skip.whole > 128) {
            lkIdx = (((int16_t)lk->opCode.d.displHigh << 8) + lk->remainder.displLow);
            lk = face->getLigKernStep(lkIdx);
        }
        GlyphCode code = *glyphCode2 & GLYPH_CODE_MASK;
        bool first = true;
        do {
            if (!first)
                lk++;
            else
                first = false;

            if (lk->nextChar == code) {
                if (lk->opCode.d.isAKern) {
                    *kern = face->getKern(
                        (((int16_t)lk->opCode.d.displHigh << 8) + lk->remainder.displLow));
                    return false;
                } else {
                    *glyphCode2 = lk->remainder.replacementChar;
                    return true;
                }
            }
        } while (!lk->skip.s.stop);
        return false;
    }

public:
    IBMFFont(IBMFFontLow &ibmFont, const uint8_t *data, unsigned int length, int index)
        : Font(FontType::IBMF), font(&ibmFont) {
        if (!font->isInitialized()) {
            if (!font->load((MemoryPtr)data, length)) {
                LOGE("Unable to initialize an IBMFFont!!");
            }
        }
        if (font->isInitialized()) {
            if ((face = font->getFace(index)) == nullptr) {
                LOGE("Internal error!!");
            }
        }
    }

    inline bool isInitialized() const { return (face != nullptr) && face->isInitialized(); }

    inline const IBMFFaceLow *getFace() { return face; }

    inline void setResolution(PixelResolution res) {
        if (face != nullptr) face->setResolution(res);
    }

    inline PixelResolution getResolution() const {
        return (face != nullptr) ? face->getResolution() : default_resolution;
    }

    inline int yAdvance() const {
        // max-hight is the maximum hight in pixels of all the glyph from the baseline
        // descender-height is the negative number of pixels below the baseline
        return isInitialized() ? ((int)face->getMaxHight()) - face->getDescenderHeight() : 0;
    }

    typedef std::function<void(GlyphCode, FIX16)> const &LigKernMappingHandler;

    /// @brief Ligature/Kerning/UTF8 Mapper
    ///
    /// Iterates on each UTF8 character present in **line**, sending to the
    /// **handler** the corresponding GlyphCode and kerning values after
    /// applying the LigKern program to the character.
    ///
    /// @param line In. The UTF8 compliant string of character.
    /// @param handler Call. The callback closure handler.
    ///
    void ligKernUTF8Map(const std::string &line, LigKernMappingHandler handler) const {
        if (line.length() != 0) {
            UTF8Iterator iter = line.begin();
            GlyphCode glyphCode1 = face->translate(*iter++);
            GlyphCode glyphCode2 = (iter == line.end()) ? NO_GLYPH_CODE : face->translate(*iter++);
            FIX16 kern;
            while (glyphCode1 != NO_GLYPH_CODE) {
                kern = (FIX16)0;
                // Ligature loop
                while (ligKern(glyphCode1, &glyphCode2, &kern)) {
                    glyphCode1 = glyphCode2;
                    glyphCode2 = (iter == line.end()) ? NO_GLYPH_CODE : face->translate(*iter++);
                }
                (handler)(glyphCode1, kern);
                glyphCode1 = glyphCode2;
                glyphCode2 = (iter == line.end()) ? NO_GLYPH_CODE : face->translate(*iter++);
            }
        }
    }

    void drawSingleLineOfText(IBMFDefs::Bitmap &canvas, IBMFDefs::Pos pos,
                              const std::string &line) const {
        if (isInitialized()) {
            IBMFDefs::Pos atPos = pos;
            Glyph glyph;
            glyph.bitmap = canvas;

            ligKernUTF8Map(line, [this, &glyph, &atPos](GlyphCode glyphCode, FIX16 kern) {
                if (this->face->getGlyph(glyphCode, glyph, true, false, atPos)) {
                    atPos.x += (glyph.metrics.advance + kern) >> 6;
                } else {
                    LOGW("Unable to retrieve glyph for glyphCode %d", glyphCode);
                }
            });
        }
    }

    void printFormatted(IBMFDefs::Bitmap &canvas, IBMFDefs::Pos pos, char *fmt, ...) const {
        if (isInitialized()) {
            va_list args;
            va_start(args, fmt);

            char *buffer = new char[MAX_SIZE];
            if (buffer != nullptr) {
                vsnprintf(buffer, MAX_SIZE, fmt, args);
                drawSingleLineOfText(canvas, pos, buffer);
            }
            delete[] buffer;
        }
    }

    IBMFDefs::Dim getTextBounds(const std::string &buffer) {
        IBMFDefs::Dim dim = IBMFDefs::Dim(0, 0);
        if (isInitialized()) {
            ligKernUTF8Map(buffer, [this, &dim](GlyphCode glyphCode, FIX16 kern) {
                IBMFDefs::Glyph glyph;
                if (face->getGlyph(glyphCode, glyph, false)) { // retrieves only the metrics
                    LOGD("Advance value: %f, yoff value: %d",
                         IBMFFaceLow::fromFIX16(glyph.metrics.advance), glyph.metrics.yoff);
                    dim.width += (glyph.metrics.advance + kern) >> 6;
                    dim.height = (dim.height > glyph.metrics.yoff)
                                     ? glyph.metrics.yoff
                                     : dim.height; // yoff is negative...
                } else {
                    LOGW("Unable to retrieve glyph for glyphCode %d", glyphCode);
                }
            });
        }
        dim.height = -dim.height;
        LOGD("IBMFFont: TextBound: width: %d, height: %d", dim.width, dim.height);
        return dim;
    }

    int getTextWidth(const std::string &buffer) {
        int width = 0;
        if (isInitialized()) {
            ligKernUTF8Map(buffer, [this, &width](GlyphCode glyphCode, FIX16 kern) {
                IBMFDefs::Glyph glyph;
                if (face->getGlyph(glyphCode, glyph, false)) { // retrieves only the metrics
                    LOGD("Advance value: %f, yoff value: %d",
                         IBMFFaceLow::fromFIX16(glyph.metrics.advance), glyph.metrics.yoff);
                    width += (glyph.metrics.advance + kern) >> 6;
                } else {
                    LOGW("Unable to retrieve glyph for glyphCode %d", glyphCode);
                }
            });
        }
        LOGD("IBMFFont: TextWidth: %d", width);
        return width;
    }

    int getTextHeight(const std::string &buffer) {
        int height = 0;
        if (isInitialized()) {
            // for (UTF8Iterator chrIter = buffer.begin(); chrIter != buffer.end(); chrIter++) {
            //     IBMFDefs::Glyph glyph;
            //     if (face->getGlyph(*chrIter, glyph, false)) {
            //         // LOGD("yoff value: %d", glyph.metrics.yoff);
            //         if (height > glyph.metrics.yoff)
            //             height = glyph.metrics.yoff; // yoff is negative
            //     } else {
            //         LOGW("Unable to retrieve glyph for char %d(%c)", *chrIter, *chrIter);
            //     }
            // }
            height = face->getEmHeight();
        }
        LOGD("IBMFFont: TextHeight: %d", height);
        return height;
    }
};
