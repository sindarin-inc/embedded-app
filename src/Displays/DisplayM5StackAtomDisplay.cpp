#if CONFIG_DISPLAY_M5STACK_ATOM_DISPLAY

#include "DisplayM5StackAtomDisplay.hpp"

#include "Misc/Dispatch.hpp"
#include "Misc/Platform.hpp"
#include "config.h"

auto DisplayM5StackAtomDisplay::setup() -> bool {
    DisplaySystem::setup();

    driver_.begin();
    driver_.startWrite();

    return true;
}

void DisplayM5StackAtomDisplay::updateDisplay(DisplaySystem::UpdateType updateType,
                                              bool clearFrameBuffer, int vcomNudge,
                                              DisplaySystem::UpdateType altUpdateType,
                                              ScreenSide screen, Rect altRect) {
    auto queue = DispatchGetUiQueue(false);
    if (queue == nullptr || !queue->isInQueue()) {
        log_w("updateDisplay called from outside the UI queue.");
    }

    PROFILE_START(updateDisplay);
    driver_.setColorDepth(8);
    for (int x = 0; x < ATOMDISPLAY_X_PIXELS; x++) {
        for (int y = 0; y < ATOMDISPLAY_Y_PIXELS; y++) {
            bool color = gfxCanvas_.getPixel(x, y);
            driver_.writePixel(x, y, color ? BLACK : WHITE);
        }
    }
    PROFILE_END_LOG_MS(updateDisplay);

    // Clear buffer afterwards if argument not set to false.
    if (clearFrameBuffer) {
        fillScreen(bg);
    }
}

#endif
