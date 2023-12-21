#pragma once

#include <cstring>
#include <memory>
#include <new>

#include "IBMFDefs.hpp"
#include "RLEExtractor.hpp"

using namespace ibmf_defs;

/**
 * @brief Access to a IBMF face.
 *
 * This is a low-level class to allow acces to a IBMF faces generated through METAFONT
 *
 */
class IBMFFaceLow {

private:
    static constexpr char const *TAG = "IBMFFaceLow";

    bool initialized_{false};
    FontFormat fontFormat_{FontFormat::UNKNOWN};
    PixelResolution resolution_{DEFAULT_RESOLUTION};

    FaceHeaderPtr faceHeader_{nullptr};
    GlyphsPixelPoolIndexes glyphsPixelPoolIndexes_{nullptr};
    GlyphsInfoPtr glyphsInfo_{nullptr};
    PixelsPoolPtr pixelsPool_{nullptr};
    LigKernStepsPtr ligKernSteps_{nullptr};
    bool opticalKerning{true};

    static CJKGlyphCodes cjkGlyphCodes;

    static inline auto isACJKGlyphCode(GlyphCode glyphCode) -> bool {
        bool res = false;
        if (cjkGlyphCodes[0].low != 0) {
            res = ((glyphCode >= cjkGlyphCodes[0].low) && (glyphCode <= cjkGlyphCodes[0].high)) ||
                  ((glyphCode >= cjkGlyphCodes[1].low) && (glyphCode <= cjkGlyphCodes[1].high)) ||
                  ((glyphCode >= cjkGlyphCodes[2].low) && (glyphCode <= cjkGlyphCodes[2].high));
        }
        return res;
    }

public:
    IBMFFaceLow() = default;

    ~IBMFFaceLow() = default;

    inline static auto fromFIX16(FIX16 val) -> float { return (float)val / 64.0; }
    inline static auto toFIX16(float val) -> FIX16 { return (FIX16)(val * 64.0); }
    inline static auto fromFIX14(FIX14 val) -> float { return (float)toFIX16(val) / 64.0; }

    // ---

    auto load(const MemoryPtr dataStart, const int dataLength, FontFormat fontFmt) -> bool;

    [[nodiscard]] inline auto isInitialized() const -> bool { return initialized_; }
    [[nodiscard]] inline auto getFacePtSize() const -> uint8_t { return faceHeader_->pointSize; }
    [[nodiscard]] inline auto getLineHeight() const -> uint16_t { return faceHeader_->lineHeight; }
    [[nodiscard]] inline auto getEmHeight() const -> uint16_t { return faceHeader_->emHeight >> 6; }
    [[nodiscard]] inline auto getDescenderHeight() const -> int16_t {
        return -(int16_t)faceHeader_->descenderHeight;
    }
    [[nodiscard]] inline auto getLigKernStep(uint16_t idx) const -> LigKernStep * {
        return &(*ligKernSteps_)[idx];
    }
    [[nodiscard]] inline auto getResolution() const -> PixelResolution { return resolution_; }

    [[nodiscard]] inline auto getGlyphHOffset(GlyphCode glyphCode) const -> int8_t {
        if (glyphCode >= faceHeader_->glyphCount) {
            return 0;
        }
        return (*glyphsInfo_)[glyphCode].horizontalOffset;
    }

    [[nodiscard]] inline auto getGlyphWidth(GlyphCode glyphCode) const -> uint8_t {
        if (glyphCode < faceHeader_->glyphCount) {
            return (*glyphsInfo_)[glyphCode].bitmapWidth;
        } else if (glyphCode == SPACE_CODE) {
            return static_cast<FIX16>(static_cast<uint16_t>(faceHeader_->spaceSize));
        } else {
            return 0;
        }
    }

    [[nodiscard]] inline auto getLigKernPgmIndex(GlyphCode glyphCode) const -> uint16_t {

        return (glyphCode < faceHeader_->glyphCount) ? (*glyphsInfo_)[glyphCode].ligKernPgmIndex
                                                     : NO_LIG_KERN_PGM;
    }

    inline auto setOpticalkerning(bool value) { opticalKerning = value; }

    inline auto isOpticalKerningEnabled() const -> bool { return opticalKerning; }

    inline auto setResolution(PixelResolution res) -> void { resolution_ = res; }

    auto ligKern(const GlyphCode glyphCode1, GlyphCode *glyphCode2, FIX16 *kern) -> bool;

    auto getGlyph(GlyphCode glyphCode, Glyph &appGlyph, bool loadBitmap, bool caching = true,
                  Pos atPos = Pos(0, 0), bool inverted = false) -> bool;

    auto getGlyphMetrics(GlyphCode glyphCode, Glyph &appGlyph) -> bool;

    auto showBitmap(const Bitmap &bitmap) const -> void;
    auto showGlyph(const Glyph &glyph, GlyphCode glyphCode, char32_t codePoint = ' ') const -> void;

    inline auto showGlyph2(const Glyph &glyph, char32_t codePoint) const -> void {
        showGlyph(glyph, 0, codePoint);
    }

    auto showGlyphInfo(GlyphCode i, const GlyphInfo &g) const -> void;
    auto showLigKerns() const -> void;
    auto showFace() const -> void;

    inline auto getGlyphHorizontalMetrics(GlyphCode glyphCode, int16_t *xoff, FIX16 *advance)
        -> bool {

        if ((glyphCode == SPACE_CODE) ||
            ((*glyphsInfo_)[glyphCode].bitmapWidth == 0)) { // send as a space character
            *xoff = 0;
            *advance = static_cast<FIX16>(static_cast<uint16_t>(faceHeader_->spaceSize) << 6);
            return true;
        }

        if (glyphCode >= faceHeader_->glyphCount) {
            return false;
        }

        GlyphInfo *glyphInfo = &(*glyphsInfo_)[glyphCode];

        if (glyphInfo == nullptr) {
            return false;
        }

        FIX16 adj = glyphInfo->advance < ((glyphInfo->bitmapWidth + 1) << 6) ? (1 << 6) : 0;

        // NOLINTNEXTLINE(bugprone-signed-char-misuse, cert-str34-c)
        *xoff = static_cast<int16_t>(glyphInfo->horizontalOffset);
        *advance = glyphInfo->advance + adj;

        return true;
    }

    static inline void setCJKGlyphCodeRange(int index, GlyphCode low, GlyphCode high) {
        // LOGD("CJK GlyphCode Range Received: %" PRIu16 " - %" PRIu16, low, high);

        if ((low >= DONT_CARE_CODE) || (high >= DONT_CARE_CODE)) {
            low = high = 0;
        }
        cjkGlyphCodes[index].low = low;
        cjkGlyphCodes[index].high = high;

        // LOGD("CJK GlyphCode Range Computed: %" PRIu16 " - %" PRIu16, low, high);
    }
};
