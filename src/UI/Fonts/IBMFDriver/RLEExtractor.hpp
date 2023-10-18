#pragma once

#include <cinttypes>
#include <cstring>
#include <iostream>

#include "IBMFDefs.hpp"

using namespace ibmf_defs;

class RLEExtractor {
private:
    static constexpr char const *TAG = "RLEExtractor";
    MemoryPtr fromPixelsPtr_{nullptr}, fromPixelsEnd_{nullptr};

    uint32_t repeatCount_{0};

    uint8_t nybbleFlipper_ = 0xf0U;
    uint8_t nybbleByte_{0};

    PixelResolution resolution_;

    static constexpr uint8_t PK_REPEAT_COUNT = 14;
    static constexpr uint8_t PK_REPEAT_ONCE = 15;

    inline auto getnext8(uint8_t &val) -> bool {
        if (fromPixelsPtr_ >= fromPixelsEnd_) {
            return false;
        }
        val = *fromPixelsPtr_++;
        return true;
    }

    auto getNybble(uint8_t &nyb) -> bool;
    auto getPackedNumber(uint32_t &val, const RLEMetrics &rleMetrics) -> bool;

    inline auto copyOneRowEightBits(MemoryPtr fromLine, MemoryPtr toLine, int16_t fromCol,
                                    int size) const -> void {
        memcpy(toLine + fromCol, fromLine + fromCol, size);
    }

    auto copyOneRowOneBit(MemoryPtr fromLine, MemoryPtr toLine, int16_t fromCol, int16_t endCol,
                          bool inverted) const -> void;

public:
    RLEExtractor(PixelResolution resolution) : resolution_(resolution) {}

    auto retrieveBitmap(const RLEBitmap &fromBitmap, Bitmap &toBitmap, const Pos atOffset,
                        const RLEMetrics rleMetrics, bool inverted) -> bool;
};