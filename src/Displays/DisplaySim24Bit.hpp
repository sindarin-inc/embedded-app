#pragma once

#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>

#include "DisplaySystem.hpp"
#include "sindarin-debug.h"

class DisplaySim24Bit : public DisplaySystem {
public:
    DisplaySim24Bit(uint16_t w, uint16_t h) noexcept : gfxCanvas_(w, h), width_(w), height_(h) {
        gfxClass_ = &gfxCanvas_;
        buffer_ = std::make_unique<uint8_t[]>(w * h * 3); // 3 bytes per pixel for 24-bit color
    };

    DisplaySim24Bit(DisplaySim24Bit const &) = delete;

    ~DisplaySim24Bit() override {
        if (renderQueue_ != nullptr) {
            vQueueDelete(renderQueue_);
        }
    };

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

    // Simulator-specific methodds
    auto renderQueue() -> QueueHandle_t override { return renderQueue_; };

    auto buffer() -> std::unique_ptr<uint8_t[]> & override { return buffer_; };

    auto getSkipSignalling() -> bool override { return skipSignalling_; };

    void setSkipSignalling(bool skip) override { skipSignalling_ = skip; };

protected:
    [[nodiscard]] auto nativeWidth() const -> uint16_t override { return width_; }
    [[nodiscard]] auto nativeHeight() const -> uint16_t override { return height_; }

    [[nodiscard]] auto getBuffer(void) -> uint8_t * override {
        return reinterpret_cast<uint8_t *>(gfxCanvas_.getBuffer());
    }

    GFXcanvas24 gfxCanvas_;
    uint16_t width_;
    uint16_t height_;
    std::unique_ptr<uint8_t[]> buffer_;
    bool skipSignalling_ = false;
    bool fullRefreshNext_ = false;
    QueueHandle_t renderQueue_ = nullptr;
};
