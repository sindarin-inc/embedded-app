#pragma once

#include "DisplaySystem.hpp"
#include "sindarin-debug.h"

#define DisplaySDL_X_PIXELS 256
#define DisplaySDL_Y_PIXELS 256

class DisplaySDLSimulator : public DisplaySystem {
public:
    DisplaySDLSimulator();
    ~DisplaySDLSimulator() = default;

    bool setup() override;
    void updateDisplay(DisplaySystem::UpdateType updateType) override;

    void setLighting(unsigned int newLighting) { lighting_ = newLighting; }

protected:
    GFXcanvas1 gfxCanvas_;
    unsigned int lighting_ = 21;
};
