#pragma once

#include "DisplaySystem.hpp"
#include "Waveshare6inHD.hpp"
#include "sindarin-debug.h"

class DisplayWaveshare6inHD : public DisplaySystem {
public:
    DisplayWaveshare6inHD() noexcept
        : gfxCanvas_(WAVESHARE_6INHD_X_PIXELS, WAVESHARE_6INHD_Y_PIXELS),
          driver_(DISPLAY_WAVESHARE_6INHD_CS, DISPLAY_WAVESHARE_6INHD_RESET) {
        gfxClass_ = &gfxCanvas_;
    };
    DisplayWaveshare6inHD(DisplayWaveshare6inHD const &) = delete;

    ~DisplayWaveshare6inHD() override = default;

    [[nodiscard]] auto defaultTextInsets() const -> DisplayInset override {
        return {300, 300, 20, 20};
    };
    [[nodiscard]] auto minTextInsets() const -> DisplayInset override { return {5, 5, 5, 5}; };
    [[nodiscard]] auto minTextBoxWidth() const -> int16_t override { return 125; };
    [[nodiscard]] auto minTextBoxHeight() const -> int16_t override { return 45; };

    auto setup() -> bool override;

    void updateDisplay(DisplaySystem::UpdateType updateType = UNSPECIFIED,
                       bool clearFrameBuffer = true, int vcomNudge = 0,
                       DisplaySystem::UpdateType altUpdateType = UNSPECIFIED,
                       ScreenSide screen = ScreenSide::Both,
                       Rect altRect = {{0, 0}, {0, 0}}) override;

protected:
    [[nodiscard]] auto nativeWidth() const -> uint16_t override { return WAVESHARE_6INHD_X_PIXELS; }

    [[nodiscard]] auto nativeHeight() const -> uint16_t override {
        return WAVESHARE_6INHD_Y_PIXELS;
    }

    [[nodiscard]] auto getBuffer() -> uint8_t * override { return gfxCanvas_.getBuffer(); }

    GFXcanvas1 gfxCanvas_;
    Waveshare6inHD driver_;
};
