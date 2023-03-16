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

    bool initialized;
    PreamblePtr preamble;
    IBMFFaceLow faces[MAX_FACE_COUNT];

public:
    IBMFFontLow(MemoryPtr fontData, uint32_t length) {

        initialized = load(fontData, length);
        if (!initialized) {
            LOGE("Font data not recognized!");
        }
    }

    IBMFFontLow() : initialized(false) {}
    ~IBMFFontLow() {}

    inline FontFormat getFontFormat() const {
        return (isInitialized()) ? preamble->bits.fontFormat : FontFormat::UNKNOWN;
    }
    inline bool isInitialized() const { return initialized; }

    inline IBMFFaceLowPtr getFace(int idx) {
        return (isInitialized() && (idx < preamble->faceCount)) ? &faces[idx] : nullptr;
    }

    bool load(const MemoryPtr fontData, uint32_t length) {

        MemoryPtr data = fontData;

        preamble = (Preamble *)data;
        data += sizeof(Preamble);

        if constexpr (IBMF_TRACING)
            LOGD("Loading font at location 0x%p of length %d", fontData, length);

        if (strncmp("IBMF", preamble->marker, 4) != 0) {
            LOGE("Preamble in error: IBMF marker absent!!");
            return false;
        }
        if (preamble->bits.version != IBMF_VERSION) {
            LOGE("Font is of a wrong version. Expected V4, got V%d", preamble->bits.version);
            return false;
        }

        if (preamble->faceCount >= 10) {
            LOGE("Too many faces in the font. Limit is set to %d in IBMFDefs.h", MAX_FACE_COUNT);
            return false;
        }
        uint32_t *binFaceOffsets = (uint32_t *)data;

        uint32_t binFaceLengths[MAX_FACE_COUNT];
        for (int i = 0; i < preamble->faceCount; i++) {
            binFaceLengths[i] = ((i < (preamble->faceCount - 1)) ? binFaceOffsets[i + 1] : length) -
                                binFaceOffsets[i];
        }

        for (int i = 0; i < preamble->faceCount; i++) {
            if (!faces[i].load(fontData + binFaceOffsets[i], binFaceLengths[i],
                               preamble->bits.fontFormat)) {
                LOGE("Unable to load face %d", i);
                return false;
            }
        }
        initialized = true;
        return true;
    }
};
