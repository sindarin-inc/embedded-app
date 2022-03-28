#pragma once

#include <WROVER_KIT_LCD.h>

#include "DisplaySystem.hpp"
#include "sindarin-debug.h"

class DisplayWroverLcd : public DisplaySystem, WROVER_KIT_LCD {
public:
    DisplayWroverLcd();
    ~DisplayWroverLcd() = default;

    void begin() { WROVER_KIT_LCD::begin(); }
    auto setup() -> bool override;
    void updateDisplay(DisplaySystem::UpdateType updateType) override;

    void invertDisplay(bool newInvert) { DisplaySystem::invertDisplay(newInvert); }

protected:
    // Would be neat to use GFXcanvas16 here and support full color and the native format of
    // the display, but it takes too much ram and GFXcanvas16 doesn't have a super easy way
    // to override and use the PSRAM to allocate the buffer.
    GFXcanvas1 gfxCanvas_;
};
