#pragma once

#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>

#include "DisplaySystem.hpp"
#include "sindarin-debug.h"

class DisplaySim8Bit : public DisplaySystem {
public:
    DisplaySim8Bit(uint16_t w, uint16_t h) noexcept : gfxCanvas_(w, h) {
        gfxClass_ = &gfxCanvas_;
        canvasBuffer_ = gfxCanvas_.getBuffer();
        canvasWidth_ = w;
        canvasHeight_ = h;

        defaultXPadding = 100;
        defaultYPadding = 20;
        defaultScreenWidth = canvasWidth_;
        defaultScreenHeight = canvasHeight_;
        maxBoundingBox = {5, 5, static_cast<uint32_t>(defaultScreenWidth) - 10,
                          static_cast<uint32_t>(defaultScreenHeight) - 10};
        minBoundingBox = {0, 0, 125, 45};
        defaultBoundingBox = {defaultXPadding, defaultYPadding,
                              defaultScreenWidth - static_cast<uint32_t>(defaultXPadding) * 2,
                              defaultScreenHeight - static_cast<uint32_t>(defaultYPadding) * 2};

        buffer_ = std::make_unique<uint8_t[]>(w * h);
    };
    DisplaySim8Bit(DisplaySim8Bit const &) = delete;
    ~DisplaySim8Bit() override {
        if (renderQueue_ != nullptr) {
            vQueueDelete(renderQueue_);
        }
    };

    auto setup() -> bool override;

    void updateDisplay(DisplaySystem::UpdateType updateType = UNSPECIFIED,
                       bool clearFrameBuffer = true, int vcomNudge = 0,
                       DisplaySystem::UpdateType altUpdateType = UNSPECIFIED,
                       ScreenSide screen = ScreenSide::Both,
                       Rect altRect = {{0, 0}, {0, 0}}) override;

    // Simulator-specific methodds
    auto renderQueue() -> QueueHandle_t override { return renderQueue_; };

    auto buffer() -> std::unique_ptr<uint8_t[]> & override { return buffer_; };

    auto getSkipSignalling() -> bool override { return skipSignalling_; };

    void setSkipSignalling(bool skip) override { skipSignalling_ = skip; };

protected:
    std::unique_ptr<uint8_t[]> buffer_;
    bool skipSignalling_ = false;
    GFXcanvas8 gfxCanvas_;
    bool fullRefreshNext_ = false;
    QueueHandle_t renderQueue_ = nullptr;
};
