#pragma once

#include "DisplaySystem.hpp"
#include "Waveshare6inHD.hpp"
#include "sindarin-debug.h"

class DisplayWaveshare6inHD : public DisplaySystem {
public:
    DisplayWaveshare6inHD() noexcept
        : gfxCanvas_(WAVESHARE_6INHD_X_PIXELS, WAVESHARE_6INHD_Y_PIXELS),
          driver_(CONFIG_DISPLAY_WAVESHARE_6INHD_CS, CONFIG_DISPLAY_WAVESHARE_6INHD_RESET) {
        gfxClass_ = &gfxCanvas_;
    };
    DisplayWaveshare6inHD(DisplayWaveshare6inHD const &) = delete;

    ~DisplayWaveshare6inHD() override = default;

    [[nodiscard]] auto defaultTextInsets() const -> DisplayInset override {
        return {150, 150, 40, 40};
    };
    [[nodiscard]] auto minTextInsets() const -> DisplayInset override { return {5, 5, 5, 5}; };
    [[nodiscard]] auto minTextBoxWidth() const -> int16_t override { return 125; };
    [[nodiscard]] auto minTextBoxHeight() const -> int16_t override { return 45; };

    auto setup() -> bool override;

    void updateDisplay(DisplaySystem::UpdateType updateType = UNSPECIFIED,
                       bool clearFrameBuffer = true, int vcomNudge = 0,
                       DisplaySystem::UpdateType altUpdateType = UNSPECIFIED,
                       ScreenSide screen = ScreenSide::Both,
                       SolRect altRect = {{0, 0}, {0, 0}}) override;

protected:
    [[nodiscard]] auto nativeWidth() const -> uint16_t override { return WAVESHARE_6INHD_X_PIXELS; }

    [[nodiscard]] auto nativeHeight() const -> uint16_t override {
        return WAVESHARE_6INHD_Y_PIXELS;
    }

    [[nodiscard]] auto getBuffer() -> uint8_t * override { return gfxCanvas_.getBuffer(); }
#if CONFIG_WAVESHARE_8BIT
    GFXcanvas8 gfxCanvas_;
#else
    GFXcanvas1 gfxCanvas_;
#endif
    Waveshare6inHD driver_;
};
