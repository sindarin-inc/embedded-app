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
    IBMFFaceLow *faces;

public:
    IBMFFontLow(MemoryPtr fontData, uint32_t length) {

        initialized = load(fontData, length);
        if (!initialized) {
            log_e("%s: Font data not recognized!", TAG);
        }
    }

    IBMFFontLow() : initialized(false) {}
    ~IBMFFontLow() {}

    inline uint8_t getCharSet() const { return preamble->bits.char_set; }
    inline bool isInitialized() const { return initialized; }

    inline IBMFFaceLowPtr getFace(int idx) const {
        return (isInitialized() && (idx < preamble->face_count)) ? &faces[idx] : nullptr;
    }

    bool load(const MemoryPtr fontData, uint32_t length) {

        MemoryPtr data = fontData;

        preamble = (Preamble *)data;
        data += sizeof(Preamble);

        if (strncmp("IBMF", preamble->marker, 4) != 0) return false;
        if (preamble->bits.version != IBMF_VERSION) return false;

        uint32_t *bin_face_offsets = (uint32_t *)data;
        data += 4 * preamble->face_count;

        uint32_t *bin_face_end_offsets = new uint32_t[preamble->face_count];
        for (int i = 0; i < preamble->face_count; i++) {
            bin_face_end_offsets[i] =
                (i < (preamble->face_count - 1)) ? bin_face_offsets[i + 1] : length;
        }

        faces = new IBMFFaceLow[preamble->face_count];
        for (int i = 0; i < preamble->face_count; i++) {
            if (!faces[i].load(fontData + bin_face_offsets[i], fontData + bin_face_end_offsets[i]))
                return false;
        }

        initialized = true;
        return true;
    }
};
