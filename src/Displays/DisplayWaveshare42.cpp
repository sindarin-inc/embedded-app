#include "Displays/DisplayWaveshare42.hpp"

#include "Communications/USBSerial.hpp"
#include "Misc/Platform.hpp"
#include "config.h"

extern bool mirrorToSerial;

auto DisplayWaveshare42::setup() -> bool {

    DisplaySystem::setup();
    driver_.Init();
    driver_.Clear();
    return true;
}

void DisplayWaveshare42::updateDisplay(DisplaySystem::UpdateType updateType, bool clearFrameBuffer,
                                       int vcomNudge, DisplaySystem::UpdateType altUpdateType,
                                       ScreenSide screen, Rect altRect) {
    if (mirrorToSerial) {
        // MAGIC NUMBER FIX ME !
        USBSerial::getInstance().txSendData(
            reinterpret_cast<const uint8_t *>(gfxCanvas_.getBuffer()),
            WAVESHARE42_X_PIXELS * WAVESHARE42_Y_PIXELS / 8);
    }

    // Ignore all the non full updates now.
    if (updateType == DisplaySystem::UpdateType::FULL ||
        updateType == DisplaySystem::UpdateType::PARTIAL) {
        // base_draw();
        log_i("updating the display");
        driver_.Display(gfxCanvas_.getBuffer());
    }

    // Clear buffer afterwards if argument not set to false.
    if (clearFrameBuffer) {
        driver_.Clear();
        fillScreen(bg);
    }
}