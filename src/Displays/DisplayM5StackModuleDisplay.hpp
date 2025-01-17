#pragma once

#include <M5GFX.h>
#include <M5ModuleDisplay.h>

#include "DisplaySystem.hpp"
#include "sindarin-debug.h"

#define MODULEDISPLAY_X_PIXELS 1920
#define MODULEDISPLAY_Y_PIXELS 1080

class DisplayM5StackModuleDisplay : public DisplaySystem {
public:
    DisplayM5StackModuleDisplay() noexcept
        : gfxCanvas_(MODULEDISPLAY_X_PIXELS, MODULEDISPLAY_Y_PIXELS),
          driver_(MODULEDISPLAY_X_PIXELS, MODULEDISPLAY_Y_PIXELS) {
        gfxClass_ = &gfxCanvas_;
    };
    DisplayM5StackModuleDisplay(DisplayM5StackModuleDisplay const &) = delete;

    ~DisplayM5StackModuleDisplay() override = default;

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
                       SolRect altRect = {{0, 0}, {0, 0}}) override;

protected:
    [[nodiscard]] auto nativeWidth() const -> uint16_t override { return MODULEDISPLAY_X_PIXELS; }

    [[nodiscard]] auto nativeHeight() const -> uint16_t override { return MODULEDISPLAY_Y_PIXELS; }

    [[nodiscard]] auto getBuffer() -> uint8_t * override { return gfxCanvas_.getBuffer(); }

    GFXcanvas8 gfxCanvas_;
    M5ModuleDisplay driver_;
};
