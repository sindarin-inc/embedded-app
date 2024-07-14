#pragma once

#include <M5AtomDisplay.h>
#include <M5GFX.h>

#include "DisplaySystem.hpp"
#include "sindarin-debug.h"

#define ATOMDISPLAY_X_PIXELS 1280
#define ATOMDISPLAY_Y_PIXELS 720

class DisplayM5StackAtomDisplay : public DisplaySystem {
public:
    DisplayM5StackAtomDisplay() noexcept
        : gfxCanvas_(ATOMDISPLAY_X_PIXELS, ATOMDISPLAY_Y_PIXELS),
          driver_(ATOMDISPLAY_X_PIXELS, ATOMDISPLAY_Y_PIXELS) {
        gfxClass_ = &gfxCanvas_;
    };
    DisplayM5StackAtomDisplay(DisplayM5StackAtomDisplay const &) = delete;

    ~DisplayM5StackAtomDisplay() override = default;

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
    [[nodiscard]] auto nativeWidth() const -> uint16_t override { return ATOMDISPLAY_X_PIXELS; }

    [[nodiscard]] auto nativeHeight() const -> uint16_t override { return ATOMDISPLAY_Y_PIXELS; }

    [[nodiscard]] auto getBuffer() -> uint8_t * override { return gfxCanvas_.getBuffer(); }

    GFXcanvas1 gfxCanvas_;
    M5AtomDisplay driver_;
};
