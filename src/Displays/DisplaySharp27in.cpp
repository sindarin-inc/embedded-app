#if CONFIG_DISPLAY_SHARP_27IN

#include "DisplaySharp27in.hpp"

#include "Communications/USBSerial.hpp"
#include "Misc/Dispatch.hpp"
#include "Misc/Platform.hpp"
#include "config.h"

extern bool mirrorToSerial;

auto DisplaySharp27in::setup() -> bool {
    DisplaySystem::setup();
    DisplaySystem::invertDisplay(true);
    return (driver_.begin());
}

void DisplaySharp27in::updateDisplay(DisplaySystem::UpdateType updateType, bool clearFrameBuffer,
                                     int vcomNudge, DisplaySystem::UpdateType altUpdateType,
                                     ScreenSide screen, SolRect altRect) {
    auto queue = DispatchGetUiQueue(false);
    if (queue == nullptr || !queue->isInQueue()) {
        log_w("updateDisplay called from outside the UI queue.");
    }

    if (mirrorToSerial) {
        USBSerial::getInstance().txSendData(
            // Note, this only works in 1-bit mode.
            // If the canvas is 8-bit, we need to convert it to 1-bit or
            // send some notif to the desktop app to handle that.
            reinterpret_cast<const uint8_t *>(gfxCanvas_.getBuffer()),
            gfxCanvas_.width() * gfxCanvas_.height() / 8);
    }

    PROFILE_START(updateDisplay);
    if (updateType == UpdateType::PARTIAL || updateType == UpdateType::FULL ||
        updateType == FAST_INDICATOR) {
        driver_.refresh(getBuffer());
    }

    PROFILE_END_LOG_MS(updateDisplay);

    // Clear buffer afterwards if argument not set to false.
    if (clearFrameBuffer) {
        fillScreen(bg);
    }
}

#endif