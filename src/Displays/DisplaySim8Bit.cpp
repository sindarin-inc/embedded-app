#if CONFIG_DISPLAY_SIM_8BIT

#include "DisplaySim8Bit.hpp"

#include "Misc/Dispatch.hpp"
#include "Misc/Platform.hpp"
#include "config.h"

auto DisplaySim8Bit::setup() -> bool {
    DisplaySystem::setup();

    if (renderQueue_ == nullptr) {
        renderQueue_ = xQueueCreate(1, sizeof(uint8_t));
    }
    xQueueReset(renderQueue_);

    return true;
}

void DisplaySim8Bit::updateDisplay(DisplaySystem::UpdateType updateType, bool clearFrameBuffer,
                                   int vcomNudge, DisplaySystem::UpdateType altUpdateType,
                                   ScreenSide screen, SolRect altRect) {
    auto queue = DispatchGetUiQueue(false);
    if (queue == nullptr || !queue->isInQueue()) {
        log_w("updateDisplay called from outside the UI queue.");
    }

    // Double buffer so we can start writing the next frame buffer while this one is getting drawn
    memcpy(buffer_.get(), gfxCanvas_.getBuffer(), gfxCanvas_.width() * gfxCanvas_.height());

    if (!skipSignalling_) {
        uint8_t in = 1;
        xQueueSend(renderQueue_, &in, portMAX_DELAY);
    }

    // Clear buffer afterwards if argument not set to false.
    if (clearFrameBuffer) {
        fillScreen(bg);
    }

    fullRefreshNext_ = false;
}

#endif
