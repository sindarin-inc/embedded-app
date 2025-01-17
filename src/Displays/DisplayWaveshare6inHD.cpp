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
#if !CONFIG_WAVESHARE_HEADLESS
    driver_.init();
#endif
    return true;
}

void DisplayWaveshare6inHD::updateDisplay(DisplaySystem::UpdateType updateType,
                                          bool clearFrameBuffer, int vcomNudge,
                                          DisplaySystem::UpdateType altUpdateType,
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
    if (updateType == UpdateType::PARTIAL || updateType == UpdateType::FAST_INDICATOR) {
#if !CONFIG_WAVESHARE_HEADLESS
#if CONFIG_WAVESHARE_8BIT
        driver_.Eight_bp_Refresh(gfxCanvas_.getBuffer(), (UWORD)0, (UWORD)0,
                                 (UWORD)gfxCanvas_.width(), (UWORD)gfxCanvas_.height(), false,
                                 (UDOUBLE)0);

#else
        driver_.One_bp_Refresh(gfxCanvas_.getBuffer(), (UWORD)0, (UWORD)0,
                               (UWORD)gfxCanvas_.width(), (UWORD)gfxCanvas_.height(),
                               (uint8_t)A2_Mode, (UDOUBLE)0, true);
#endif
#endif
    } else if (updateType == UpdateType::FULL) {
#if !CONFIG_WAVESHARE_HEADLESS
#if CONFIG_WAVESHARE_8BIT
        driver_.Eight_bp_Refresh(gfxCanvas_.getBuffer(), (UWORD)0, (UWORD)0,
                                 (UWORD)gfxCanvas_.width(), (UWORD)gfxCanvas_.height(), false,
                                 (UDOUBLE)0);
#else
        driver_.One_bp_Refresh(gfxCanvas_.getBuffer(), (UWORD)0, (UWORD)0,
                               (UWORD)gfxCanvas_.width(), (UWORD)gfxCanvas_.height(),
                               (uint8_t)GC16_Mode, (UDOUBLE)0, true);
#endif
#endif
    }

    PROFILE_END_LOG_MS(updateDisplay);

    // Clear buffer afterwards if argument not set to false.
    if (clearFrameBuffer) {
        fillScreen(bg);
    }
}

#endif