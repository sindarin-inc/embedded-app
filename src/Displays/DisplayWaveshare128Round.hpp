#pragma once

#include "DisplaySystem.hpp"
#include "Waveshare128Round.hpp"
#include "sindarin-debug.h"

class DisplayWaveshare128Round : public DisplaySystem {
public:
    DisplayWaveshare128Round() noexcept
        : gfxCanvas_(WAVESHARE_128ROUND_X_PIXELS, WAVESHARE_128ROUND_Y_PIXELS),
          driver_(DISPLAY_WAVESHARE_128ROUND_CLK, DISPLAY_WAVESHARE_128ROUND_MOSI, DISPLAY_WAVESHARE_128ROUND_CS) {
        gfxClass_ = &gfxCanvas_;
    };

    DisplayWaveshare128Round(DisplayWaveshare128Round const &) = delete;

    ~DisplayWaveshare128Round() override = default;

    [[nodiscard]] auto defaultTextInsets() const -> DisplayInset override {
        return {20, 20, 10, 10};
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
    [[nodiscard]] auto nativeWidth() const -> uint16_t override { return WAVESHARE_128ROUND_X_PIXELS; }

    [[nodiscard]] auto nativeHeight() const -> uint16_t override { return WAVESHARE_128ROUND_Y_PIXELS; }

    [[nodiscard]] auto getBuffer() -> uint8_t * override { return gfxCanvas_.getBuffer(); }

    GFXcanvas8 gfxCanvas_;
    Waveshare128Round driver_;
};
