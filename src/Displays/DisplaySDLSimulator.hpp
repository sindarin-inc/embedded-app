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
};
