#ifndef DISPLAY_EINK_154IN_H
#define DISPLAY_EINK_154IN_H

#include "DisplaySystem.hpp"
#include "sindarin-debug.h"

#define EinkET013TT1_X_PIXELS 256
#define EinkET013TT1_Y_PIXELS 256

class DisplayEinkET013TT1 : public DisplaySystem {
public:
    DisplayEinkET013TT1(uint8_t cs, uint8_t oei, uint8_t rst, uint8_t busy);
    ~DisplayEinkET013TT1() override = default;

    void drawPixel(int16_t x, int16_t y, uint16_t color) override;
    void updateDisplay() override;

    // Overrides to DisplaySystem to support the EinkET013TT1 display
    // BC: Changed to never do partial_referesh for now.
    void setPartialRefresh() override { partialRefresh_ = 0; }

protected:
    // Code specific to creating a framebuffer in the format the SWTCON library wants
    uint8_t buffer_[(EinkET013TT1_X_PIXELS / 8) * EinkET013TT1_Y_PIXELS];
    void writeImage(uint8_t *fb, const unsigned char *img, uint16_t size);
    auto writePartialImage(uint16_t x, uint16_t y, const unsigned char *img, uint16_t imgW,
                           uint16_t imgH) -> uint8_t;
};

#endif