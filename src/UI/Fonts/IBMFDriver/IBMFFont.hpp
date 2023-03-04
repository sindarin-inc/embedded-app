#pragma once

#include <cstdarg>
#include <cstdio>

#include "IBMFFontLow.hpp"
#include "UI/Fonts/Font.h"

class IBMFFont : public Font {
private:
    static constexpr char const *TAG = "IBMFFont";

    IBMFFontLow *font;
    IBMFFaceLow *face;

    // Maximum size of an allocated buffer to do vsnprintf formatting
    const int MAX_SIZE = 100;

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

    inline const IBMFFaceLow * getFace() { return face; }

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

    void drawSingleLineOfText(IBMFDefs::Bitmap &canvas, IBMFDefs::Pos pos,
                              const std::string &line) const {
        if (isInitialized()) {
            IBMFDefs::Pos atPos = pos;
            Glyph glyph;
            glyph.bitmap = canvas;
            for (auto chr : line) {
                // LOGD("IBMFFont: getGlyph of char %d(%c) at pos(%d, %d) in canvas", chr, chr,
                // pos.x,
                //      pos.y);
                if (face->getGlyph(chr, glyph, true, false, atPos)) {
                    atPos.x += glyph.metrics.advance;
                    // face->showBitmap(canvas);

                    // Glyph glyph2;
                    // face->getGlyph(chr, glyph2, true);
                    // face->showGlyph2(glyph2, chr);
                } else {
                    LOGW("Unable to retrieve glyph for char %d(%c)", chr, chr);
                }
            }
            // LOGD("Showing Canvas....%p [%d, %d]", canvas.pixels, canvas.dim.width,
            //      canvas.dim.height);
            // face->showBitmap(canvas);
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
            delete buffer;
        }
    }

    IBMFDefs::Dim getTextBounds(const std::string &buffer) {
        IBMFDefs::Dim dim = IBMFDefs::Dim(0, 0);
        if (isInitialized()) {
            for (auto chr : buffer) {
                IBMFDefs::Glyph glyph;
                if (face->getGlyph(chr, glyph, false)) {
                    LOGD("advance value: %d, yoff value: %d", glyph.metrics.advance,
                         glyph.metrics.yoff);
                    dim.width += glyph.metrics.advance;
                    dim.height = (dim.height > glyph.metrics.yoff)
                                     ? glyph.metrics.yoff
                                     : dim.height; // yoff is negative...
                } else {
                    LOGW("Unable to retrieve glyph for char %d(%c)", chr, chr);
                }
            }
        }
        dim.height = -dim.height;
        LOGD("IBMFFont: TextBound: width: %d, height: %d", dim.width, dim.height);
        return dim;
    }

    int getTextWidth(const std::string &buffer) {
        int width = 0;
        if (isInitialized()) {
            for (auto chr : buffer) {
                IBMFDefs::Glyph glyph;
                if (face->getGlyph(chr, glyph, false)) {
                    // LOGD("advance value: %d", glyph.metrics.advance);
                    width += glyph.metrics.advance;
                } else {
                    LOGW("Unable to retrieve glyph for char %d(%c)", chr, chr);
                }
            }
        }
        LOGD("IBMFFont: TextWidth: %d", width);
        return width;
    }

    int getTextHeight(const std::string &buffer) {
        int height = 0;
        if (isInitialized()) {
            for (auto chr : buffer) {
                IBMFDefs::Glyph glyph;
                if (face->getGlyph(chr, glyph, false)) {
                    // LOGD("yoff value: %d", glyph.metrics.yoff);
                    if (height > glyph.metrics.yoff)
                        height = glyph.metrics.yoff; // yoff is negative
                } else {
                    LOGW("Unable to retrieve glyph for char %d(%c)", chr, chr);
                }
            }
        }
        LOGD("IBMFFont: TextHeight: %d", -height);
        return -height;
    }
};
