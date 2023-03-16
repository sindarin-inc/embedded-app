#pragma once

#include <cstring>
#include <fstream>
#include <iostream>

#include "IBMFDefs.hpp"
#include "IBMFFaceLow.hpp"

using namespace IBMFDefs;

/**
 * @brief Access to a IBMF font.
 *
 * This is a low-level class to allow acces to a IBMF font generated through METAFONT
 *
 */
class IBMFFontLow {

private:
    static constexpr char const *TAG = "IBMFFontLow";

    typedef IBMFFaceLow *IBMFFaceLowPtr;

    bool initialized_;
    PreamblePtr preamble_;
    IBMFFaceLow faces_[MAX_FACE_COUNT];

public:
    IBMFFontLow(MemoryPtr fontData, uint32_t length) {

        initialized_ = load(fontData, length);
        if (!initialized_) {
            LOGE("Font data not recognized!");
        }
    }

    IBMFFontLow() : initialized_(false) {}
    ~IBMFFontLow() {}

    inline auto getFontFormat() const -> FontFormat {
        return (isInitialized()) ? preamble_->bits.fontFormat : FontFormat::UNKNOWN;
    }
    inline auto isInitialized() const -> bool { return initialized_; }

    inline auto getFace(int idx) -> IBMFFaceLowPtr {
        return (isInitialized() && (idx < preamble_->faceCount)) ? &faces_[idx] : nullptr;
    }

    auto load(const MemoryPtr fontData, uint32_t length) -> bool {

        MemoryPtr data = fontData;

        preamble_ = (Preamble *)data;
        data += sizeof(Preamble);

        if constexpr (IBMF_TRACING)
            LOGD("Loading font at location 0x%p of length %d", fontData, length);

        if (strncmp("IBMF", preamble_->marker, 4) != 0) {
            LOGE("Preamble in error: IBMF marker absent!!");
            return false;
        }
        if (preamble_->bits.version != IBMF_VERSION) {
            LOGE("Font is of a wrong version. Expected V4, got V%d", preamble_->bits.version);
            return false;
        }

        if (preamble_->faceCount >= 10) {
            LOGE("Too many faces in the font. Limit is set to %d in IBMFDefs.h", MAX_FACE_COUNT);
            return false;
        }
        uint32_t *binFaceOffsets = (uint32_t *)data;

        uint32_t binFaceLengths[MAX_FACE_COUNT];
        for (int i = 0; i < preamble_->faceCount; i++) {
            binFaceLengths[i] = ((i < (preamble_->faceCount - 1)) ? binFaceOffsets[i + 1] : length) -
                                binFaceOffsets[i];
        }

        for (int i = 0; i < preamble_->faceCount; i++) {
            if (!faces_[i].load(fontData + binFaceOffsets[i], binFaceLengths[i],
                               preamble_->bits.fontFormat)) {
                LOGE("Unable to load face %d", i);
                return false;
            }
        }
        initialized_ = true;
        return true;
    }
};
