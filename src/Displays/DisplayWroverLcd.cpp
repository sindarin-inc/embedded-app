#if WROVER_KIT

#include "DisplayWroverLcd.hpp"

// Only use the top 240 px so we have a square screen and take less memory
DisplayWroverLcd::DisplayWroverLcd()
    : gfxCanvas_(WROVER_WIDTH, WROVER_HEIGHT), DisplaySystem(), WROVER_KIT_LCD() {
    gfxClass_ = &gfxCanvas_;
}

void DisplayWroverLcd::setup() {
    DisplaySystem::setup();
    setFullWindow();
}

void DisplayWroverLcd::updateDisplay(DisplaySystem::UpdateType updateType) {
    WROVER_KIT_LCD::startWrite();
    WROVER_KIT_LCD::setAddrWindow(0, 0, gfxClass()->width(), gfxClass()->height());
    for (int x = 0; x < gfxClass()->width(); x++) {
        for (int y = 0; y < gfxClass()->height(); y++) {
            auto pixel = gfxCanvas_.getPixel(x, y);
            uint16_t color = pixel ? fg : bg;
            WROVER_KIT_LCD::writePixel(x, y, color);
        }
    }
    WROVER_KIT_LCD::endWrite();

    DisplaySystem::fillScreen(bg);
};

#endif
