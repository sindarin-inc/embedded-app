#if CONFIG_DISPLAY_NULL

#include "DisplayNull.hpp"

#include "Misc/Dispatch.hpp"
#include "Misc/Platform.hpp"
#include "config.h"

auto DisplayNull::setup() -> bool {
    DisplaySystem::setup();
    return true;
}

void DisplayNull::updateDisplay(DisplaySystem::UpdateType updateType, bool clearFrameBuffer,
                                int vcomNudge, DisplaySystem::UpdateType altUpdateType,
                                ScreenSide screen, SolRect altRect) {
    // Clear buffer afterwards if argument not set to false.
    if (clearFrameBuffer) {
        fillScreen(bg);
    }
}

#endif
