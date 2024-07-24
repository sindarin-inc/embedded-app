#if CONFIG_DISPLAY_WAVESHARE_6INHD

#include "DisplayWaveshare6inHD.hpp"

#include "Communications/USBSerial.hpp"
#include "Misc/Dispatch.hpp"
#include "Misc/Platform.hpp"
#include "config.h"

extern bool mirrorToSerial;

auto DisplayWaveshare6inHD::setup() -> bool {
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

    // driver_.init();
    //  driver_.invertDisplay(true);

    return true;
}

void DisplayWaveshare6inHD::updateDisplay(DisplaySystem::UpdateType updateType,
                                          bool clearFrameBuffer, int vcomNudge,
                                          DisplaySystem::UpdateType altUpdateType,
                                          ScreenSide screen, Rect altRect) {
    auto queue = DispatchGetUiQueue(false);
    if (queue == nullptr || !queue->isInQueue()) {
        log_w("updateDisplay called from outside the UI queue.");
    }

    // for (int x = 0; x < gfxCanvas_.width(); x += 8) {
    //     drawBox(x, 0, x + 1, gfxCanvas_.height());
    //     drawBox(x + 2, 0, x + 3, gfxCanvas_.height());
    // }
    // for (int y = 0; y < gfxCanvas_.height(); y += 8) {
    //     drawBox(0, y, gfxCanvas_.width(), y + 1);
    //     drawBox(0, y + 2, gfxCanvas_.width(), y + 3);
    // }
    if (mirrorToSerial) {
        USBSerial::getInstance().txSendData(
            reinterpret_cast<const uint8_t *>(gfxCanvas_.getBuffer()),
            gfxCanvas_.width() * gfxCanvas_.height() / 8);
    }

    // The subtraction of the %32 is because of a 4 byte align thing in the
    // driver example code!
    PROFILE_START(updateDisplay);
    if (updateType == UpdateType::PARTIAL || updateType == UpdateType::FULL) {
        // driver_.One_bp_Refresh(gfxCanvas_.getBuffer(), (UWORD)0, (UWORD)0,
        //                        (UWORD)gfxCanvas_.width(), (UWORD)gfxCanvas_.height(),
        //                        (uint8_t)A2_Mode, (UDOUBLE)0, false);
        // EPD_IT8951_Standby();
        //        driver_.Sleep();
        ;
    }
    // driver_.startWrite();
    // driver_.beginTransaction();

    // // Blast out the full buffer
    // driver_.pushImage(0, 0, gfxCanvas_.width(), gfxCanvas_.height(), gfxCanvas_.getBuffer());

    // driver_.endTransaction();
    // driver_.endWrite();

    PROFILE_END_LOG_MS(updateDisplay);

    // Clear buffer afterwards if argument not set to false.
    if (clearFrameBuffer) {
        fillScreen(bg);
    }
}

#endif