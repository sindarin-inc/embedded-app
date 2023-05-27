#pragma once

#include <Misc/SpiramAllocator.hpp>
#include <stdint.h>

class Ditherer {
public:
    Ditherer() {
        _ditherBuffer = std::shared_ptr<float[]>((float *)SpiRamMalloc(sizeof(float) * 256 * 256));
    }
    void ditherToBitmap(uint8_t *image, uint8_t *bitmap, int width, int height);

private:
    std::shared_ptr<float[]> _ditherBuffer;

    float *pixelAt(int x, int y, int width, int height);
    void ditherFloatToBitmap(uint8_t *bitmap, int width, int height);
    void convertToFloat(uint8_t *image, int width, int height);
};