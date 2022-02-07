#pragma once

#include "DisplaySystem.hpp"
#include "sindarin-debug.h"

#define DisplaySDL_X_PIXELS 256
#define DisplaySDL_Y_PIXELS 256

class DisplaySDLSimulator : public DisplaySystem {
public:
    DisplaySDLSimulator();
    ~DisplaySDLSimulator() = default;

    void updateDisplay() override;

    // Clear the display must be implemented since Eink needs it. Note this is a
    // command for the display directly (essentially), not the display buffer
    // as clearScreen gets to above.
    void clearDisplay() override {}
};
