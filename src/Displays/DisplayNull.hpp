#pragma once

#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>

#include "DisplaySystem.hpp"
#include "sindarin-debug.h"

// Display that does nothing. Useful for testing.
class DisplayNull : public DisplaySystem {
public:
    DisplayNull(uint16_t w, uint16_t h) noexcept : gfxCanvas_(w, h), width_(w), height_(h) {
        gfxClass_ = &gfxCanvas_;
    };
    DisplayNull() noexcept : DisplayNull(256, 256) {}

    DisplayNull(DisplayNull const &) = delete;

    ~DisplayNull() override = default;

    [[nodiscard]] auto defaultTextInsets() const -> DisplayInset override {
        return {static_cast<uint16_t>(30 * nativeWidth() / 256),
                static_cast<uint16_t>(30 * nativeHeight() / 256),
                static_cast<uint16_t>(20 * nativeWidth() / 256),
                static_cast<uint16_t>(20 * nativeHeight() / 256)};
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
    [[nodiscard]] auto nativeWidth() const -> uint16_t override { return width_; }
    [[nodiscard]] auto nativeHeight() const -> uint16_t override { return height_; }

    [[nodiscard]] auto getBuffer(void) -> uint8_t * override { return gfxCanvas_.getBuffer(); }

    GFXcanvas1 gfxCanvas_;
    uint16_t width_;
    uint16_t height_;
};
