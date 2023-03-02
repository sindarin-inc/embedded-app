#pragma once

#include "IBMFFontLow.hpp"
#include "UI/Fonts/Font.h"

class IBMFFont : Font {
private:
    IBMFFontLow *font;
    IBMFFaceLow *face;

public:
    IBMFFont(IBMFFontLow &ibmFont, const uint8_t *data, unsigned int len, int index)
        : Font(FontType::IBMF), font(&ibmFont) {
        if (!font->isInitialized()) {
            if (font->load((MemoryPtr)data, len)) {
                face = font->getFace(index);
            }
        }
    }

    inline bool isInitialized() const { return (face != nullptr) && face->is_initialized(); }

    inline int yAdvance() const {
        // max-hight is the maximum hight in pixels of all the glyph from the baseline
        // descender-height is the negative number of pixels below the baseline
        return isInitialized() ? ((int)face->get_max_hight()) - face->get_descender_height() : 0;
    }

    void drawSingleLineOfText(IBMFDefs::Bitmap &Canvas, IBMFDefs::Pos pos,
                              const std::string &line) {
        if (isInitialized()) {
            Pos atPos = pos;
            for (auto ch : line) {
                IBMFDefs::GlyphMetrics glyph_metrics;
                face->put_glyph(ch, glyph_metrics, Canvas, atPos);
                atPos.x += glyph_metrics.advance;
            }
        }
    }

    void printFormatted(IBMFDefs::Bitmap &Canvas, IBMFDefs::Pos pos, char *fmt, ...) {
        if (isInitialized()) {
            // TODO
        }
    }

    Dim getTextBounds(const char *buffer) {
        Dim dim = Dim(0, 0);
        if (isInitialized()) {
            for (const char *chrPtr = buffer; *chrPtr != 0; chrPtr++) {
                IBMFDefs::Glyph glyph;
                face->get_glyph(*chrPtr, glyph, false);
                dim.width += glyph.metrics.advance;
                dim.height = (dim.height < -glyph.metrics.yoff) ? -glyph.metrics.yoff : dim.height;
            }
        }
        return Dim(0, 0);
    }

    int getTextWidth(const char *buffer) {
        int width = 0;
        if (isInitialized()) {
            for (const char *chrPtr = buffer; *chrPtr != 0; chrPtr++) {
                IBMFDefs::Glyph glyph;
                face->get_glyph(*chrPtr, glyph, false);
                width += glyph.metrics.advance;
            }
        }
        return width;
    }
};
