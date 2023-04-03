#pragma once

#include <cstdarg>
#include <cstdio>
#include <functional>

#include "IBMFFontLow.hpp"
#include "UI/Fonts/Font.hpp"
#include "UTF8Iterator.hpp"

class IBMFFont : public Font {
private:
    static constexpr char const *TAG = "IBMFFont";

    IBMFFontLow *font_;
    IBMFFaceLow *face_;

    // Maximum size of an allocated buffer to do vsnprintf formatting
    static constexpr int MAX_SIZE = 100;

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
    auto ligKernUTF8Map(const std::string &line, LigKernMappingHandler handler) const -> void {
        if (line.length() != 0) {
            UTF8Iterator iter = line.begin();
            GlyphCode glyphCode1 = font_->translate(*iter++);
            GlyphCode glyphCode2 = (iter == line.end()) ? NO_GLYPH_CODE : font_->translate(*iter++);
            FIX16 kern;
            while (glyphCode1 != NO_GLYPH_CODE) {
                kern = (FIX16)0;
                // Ligature loop
                while (face_->ligKern(glyphCode1, &glyphCode2, &kern)) {
                    glyphCode1 = glyphCode2;
                    glyphCode2 = (iter == line.end()) ? NO_GLYPH_CODE : font_->translate(*iter++);
                }
                (handler)(glyphCode1, kern);
                glyphCode1 = glyphCode2;
                glyphCode2 = (iter == line.end()) ? NO_GLYPH_CODE : font_->translate(*iter++);
            }
        }
    }

public:
    IBMFFont(IBMFFontLow &ibmFont, const uint8_t *data, unsigned int length, int index) noexcept
        : Font(FontType::IBMF), font_(&ibmFont) {
        if constexpr (IBMF_TRACING) {
            LOGD("IBMFFont initialisation with data of length %d and face index %d.", length,
                 index);
        }
        if (!font_->isInitialized()) {
            if (!font_->load((MemoryPtr)data, length)) {
                LOGE("Unable to initialize an IBMFFont!!");
            }
        }
        if (font_->isInitialized()) {
            face_ = font_->getFace(index);
            if (face_ == nullptr) {
                LOGE("Internal error!!");
            }
        }
    }

    [[nodiscard]] inline auto isInitialized() const -> bool {
        return (face_ != nullptr) && face_->isInitialized();
    }

    [[nodiscard]] inline auto getFace() -> const IBMFFaceLow * { return face_; }

    inline auto setResolution(PixelResolution res) -> void {
        if constexpr (IBMF_TRACING) {
            LOGD("setResolution()");
        }
        if (face_ != nullptr) {
            face_->setResolution(res);
        }
    }

    [[nodiscard]] inline auto getResolution() const -> PixelResolution {
        if constexpr (IBMF_TRACING) {
            LOGD("getResolution()");
        }
        return (face_ != nullptr) ? face_->getResolution() : DEFAULT_RESOLUTION;
    }

    auto yAdvance() const -> int override {
        if constexpr (IBMF_TRACING) {
            LOGD("yAdvance()");
            if (!isInitialized()) {
                LOGE("Not initialized!!!");
            }
        }
        return isInitialized() ? ((int)face_->getLineHeight()) - face_->getDescenderHeight() -
                                     (face_->getLineHeight() / 4)
                               : 0;
    }

    auto drawSingleLineOfText(ibmf_defs::Bitmap &canvas, ibmf_defs::Pos pos,
                              const std::string &line) const -> void {
        if constexpr (IBMF_TRACING) {
            LOGD("drawSingleLineOfText()");
        }
        if (isInitialized()) {
            ibmf_defs::Pos atPos = pos;
            Glyph glyph;
            glyph.bitmap = canvas;

            ligKernUTF8Map(line, [this, &glyph, &atPos](GlyphCode glyphCode, FIX16 kern) {
                if (this->face_->getGlyph(glyphCode, glyph, true, false, atPos)) {
                    // As advance is positive and greather than kern, we can shift right
                    // to get rid of the fix point decimals
                    atPos.x += (glyph.metrics.advance + kern) >> 6;
                } else {
                    LOGW("Unable to retrieve glyph for glyphCode %d", glyphCode);
                }
            });
        }
    }

    auto printFormatted(ibmf_defs::Bitmap &canvas, ibmf_defs::Pos pos, char *fmt, ...) const
        -> void {
        if constexpr (IBMF_TRACING) {
            LOGD("printFormatted()");
        }
        if (isInitialized()) {
            va_list args;
            va_start(args, fmt);

            char *buffer = new char[MAX_SIZE];
            if (buffer != nullptr) {
                (void)vsnprintf(buffer, MAX_SIZE, fmt, args);
                drawSingleLineOfText(canvas, pos, buffer);
            }
            delete[] buffer;
        }
    }

    auto getTextBounds(const std::string &buffer) -> ibmf_defs::Dim {
        if constexpr (IBMF_TRACING) {
            LOGD("getTextBounds()");
        }
        ibmf_defs::Dim dim = ibmf_defs::Dim(0, 0);
        if (isInitialized()) {
            ligKernUTF8Map(buffer, [this, &dim](GlyphCode glyphCode, FIX16 kern) {
                ibmf_defs::Glyph glyph;
                if (face_->getGlyph(glyphCode, glyph, false)) { // retrieves only the metrics
                    // LOGD("Advance value: %f, yoff value: %d",
                    //      IBMFFaceLow::fromFIX16(glyph.metrics.advance), glyph.metrics.yoff);
                    dim.width += (glyph.metrics.advance + kern) >> 6;
                    dim.height = (dim.height > glyph.metrics.yoff)
                                     ? glyph.metrics.yoff
                                     : dim.height; // yoff is negative...
                } else {
                    // LOGW("Unable to retrieve glyph for glyphCode %d", glyphCode);
                }
            });
        }
        dim.height = -dim.height;
        // LOGD("IBMFFont: TextBound: width: %d, height: %d", dim.width, dim.height);
        return dim;
    }

    auto getTextWidth(const std::string &buffer) -> int {
        if constexpr (IBMF_TRACING) {
            LOGD("getTextWidth()");
        }
        int width = 0;
        if (isInitialized()) {
            ligKernUTF8Map(buffer, [this, &width](GlyphCode glyphCode, FIX16 kern) {
                ibmf_defs::Glyph glyph;
                if (face_->getGlyph(glyphCode, glyph, false)) { // retrieves only the metrics
                    // LOGD("Advance value: %f, kern: %f",
                    //     IBMFFaceLow::fromFIX16(glyph.metrics.advance),
                    //     IBMFFaceLow::fromFIX16(kern));
                    width += (glyph.metrics.advance + kern + 32) >> 6;
                } else {
                    // LOGW("Unable to retrieve glyph for glyphCode %d", glyphCode);
                }
            });
        }
        // LOGD("IBMFFont: TextWidth: %d", width);
        return width;
    }

    auto getTextHeight(const std::string &buffer) -> int {
        if constexpr (IBMF_TRACING) {
            LOGD("getTextHeight()");
        }
        int height = 0;
        if (isInitialized()) {
            // for (UTF8Iterator chrIter = buffer.begin(); chrIter != buffer.end(); chrIter++) {
            //     ibmf_defs::Glyph glyph;
            //     if (face_->getGlyph(*chrIter, glyph, false)) {
            //         // LOGD("yoff value: %d", glyph.metrics.yoff);
            //         if (height > glyph.metrics.yoff) {
            //             height = glyph.metrics.yoff; // yoff is negative
            //         }
            //     } else {
            //         LOGW("Unable to retrieve glyph for char %d(%c)", *chrIter, *chrIter);
            //     }
            // }
            height = face_->getEmHeight();
        }
        // LOGD("TextHeight: %d", height);
        return height;
    }
};
