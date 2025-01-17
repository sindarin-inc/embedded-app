#pragma once

#include "DisplaySystem.hpp"
#include "Sharp27in.hpp"
#include "sindarin-debug.h"

class DisplaySharp27in : public DisplaySystem {
public:
    DisplaySharp27in() noexcept
        : gfxCanvas_(SHARP_27IN_X_PIXELS, SHARP_27IN_Y_PIXELS),
          driver_(CONFIG_DISPLAY_SHARP_27IN_CLK, CONFIG_DISPLAY_SHARP_27IN_MOSI,
                  CONFIG_DISPLAY_SHARP_27IN_CS) {
        gfxClass_ = &gfxCanvas_;
    };

    DisplaySharp27in(DisplaySharp27in const &) = delete;

    ~DisplaySharp27in() override = default;

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
                       SolRect altRect = {{0, 0}, {0, 0}}) override;

protected:
    [[nodiscard]] auto nativeWidth() const -> uint16_t override { return SHARP_27IN_X_PIXELS; }

    [[nodiscard]] auto nativeHeight() const -> uint16_t override { return SHARP_27IN_Y_PIXELS; }

    [[nodiscard]] auto getBuffer() -> uint8_t * override { return gfxCanvas_.getBuffer(); }

    GFXcanvas1 gfxCanvas_;
    Sharp27in driver_;
};
