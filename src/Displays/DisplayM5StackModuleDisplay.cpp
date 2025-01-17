#if CONFIG_DISPLAY_M5STACK_MODULE_DISPLAY

#include "DisplayM5StackModuleDisplay.hpp"

#include "Misc/Dispatch.hpp"
#include "Misc/Platform.hpp"
#include "config.h"

auto DisplayM5StackModuleDisplay::setup() -> bool {
    DisplaySystem::setup();
    DisplaySystem::invertDisplay(true);

    // NOTE: GPIO11 isn't available on the ProS3 so you have to change i2c_scl to point to an
    // available pin (like 13) here
    // https://github.com/m5stack/M5GFX/blob/b490abf5fa9e5b43479bfa2316bdb39c0c1aa46f/src/M5ModuleDisplay.h#L134

    // auto cfg = M5.config();

    // cfg.external_speaker.module_display = true;
    // cfg.external_display.module_display = true;
    // If you want to change the resolution, uncomment the following lines and
    // adjust the values
    //  cfg.module_display.logical_width = 1280;
    //  cfg.module_display.logical_height = 720;
    //  cfg.module_display.refresh_rate = 60;

    // If you want to change the resolution, uncomment the following lines and
    // adjust the values
    //  cfg.module_display.logical_width = 1280;
    //  cfg.module_display.logical_height = 720;
    //  cfg.module_display.refresh_rate = 60;

    // M5.begin(cfg);

    // See also:
    // https://github.com/m5stack/M5Module-Display-13.2/blob/main/M5Module-Display-13.2-Demo-Core2/M5Module-Display-13.2-Demo-Core2.ino

    driver_.begin();
    driver_.setColorDepth(8);
    // driver_.invertDisplay(true);
    driver_.clear(TFT_GREEN);

    return true;
}

void DisplayM5StackModuleDisplay::updateDisplay(DisplaySystem::UpdateType updateType,
                                                bool clearFrameBuffer, int vcomNudge,
                                                DisplaySystem::UpdateType altUpdateType,
                                                ScreenSide screen, SolRect altRect) {
    auto queue = DispatchGetUiQueue(false);
    if (queue == nullptr || !queue->isInQueue()) {
        log_w("updateDisplay called from outside the UI queue.");
    }

    PROFILE_START(updateDisplay);
    driver_.startWrite();
    driver_.beginTransaction();

    // Blast out the full buffer
    driver_.pushImage(0, 0, gfxCanvas_.width(), gfxCanvas_.height(), gfxCanvas_.getBuffer());

    driver_.endTransaction();
    driver_.endWrite();

    PROFILE_END_LOG_MS(updateDisplay);

    // Clear buffer afterwards if argument not set to false.
    if (clearFrameBuffer) {
        fillScreen(bg);
    }
}

#endif
