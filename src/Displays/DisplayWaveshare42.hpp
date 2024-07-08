#pragma once

#include "DisplaySystem.hpp"
#if SIMULATOR
#include "Displays/FakeEinkET013TT1.hpp"
#else
#include "Displays/Waveshare42.hpp"
#endif
#include "sindarin-debug.h"

#define WAVESHARE42_X_PIXELS 400
#define WAVESHARE42_Y_PIXELS 300

class DisplayWaveshare42 : public DisplaySystem {
public:
    DisplayWaveshare42() : gfxCanvas_(WAVESHARE42_X_PIXELS, WAVESHARE42_Y_PIXELS) {
        gfxClass_ = &gfxCanvas_;
        canvasBuffer_ = gfxCanvas_.getBuffer();
        canvasWidth_ = WAVESHARE42_X_PIXELS;
        canvasHeight_ = WAVESHARE42_Y_PIXELS;
    };
    DisplayWaveshare42(DisplayWaveshare42 const &) = delete;
    auto setup() -> bool override;
    ~DisplayWaveshare42() override = default;
    void updateDisplay(DisplaySystem::UpdateType updateType = UNSPECIFIED,
                       bool clearFrameBuffer = true, int vcomNudge = 0,
                       DisplaySystem::UpdateType altUpdateType = UNSPECIFIED,
                       ScreenSide screen = ScreenSide::Both,
                       Rect altRect = {{0, 0}, {0, 0}}) override;

    // Clear the whole screen
    //  void clearScreen();

protected:
    GFXcanvas1 gfxCanvas_;
    Waveshare42 driver_;
};