#pragma once

#include <stdint.h>

#include "Misc/SpiramAllocator.hpp"

class Ditherer {
public:
    static auto getInstance() -> Ditherer &;

    Ditherer() { ditherBuffer_ = SpiRamArrayPtr<float>(256 * 256); }
    void ditherToBitmap(uint8_t *image, uint8_t *bitmap, int width, int height, int bytesPerLine,
                        bool reversed = false);

private:
    std::shared_ptr<float[]> ditherBuffer_;

    auto pixelAt(int x, int y, int width, int height) -> float *;
    void ditherFloatToBitmap(uint8_t *bitmap, int width, int height);
    void convertToFloat(uint8_t *image, int width, int height, int bytesPerLine, bool reversed);
};