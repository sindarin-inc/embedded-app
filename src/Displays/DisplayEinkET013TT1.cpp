#if EINKET013TT1

#include "DisplayEinkET013TT1.hpp"

#include <algorithm>

#include "Displays/CustomWaveforms.hpp"
#include "Displays/EinkWaveforms.hpp"
#include "Misc/Platform.hpp"
#include "config.h"

#define VCOM_INVERTED_MODE_DELTA 2500

// Not currently used waveforms
const EinkET013TT1Waveform FULL_REFRESH_WAVEFORM = BC_EINK_31_35C_WAVEFORM_FULL;
const EinkET013TT1Waveform ORIGINAL_FULL_REFRESH_WAVEFORM = EINK_ORIGINAL_WAVEFORM_FULL;
const EinkET013TT1Waveform ORIGINAL_PARTIAL_REFRESH_WAVEFORM = EINK_31_35C_WAVEFORM_PARTIAL;

const EinkET013TT1Waveform FULL_BIG_ORIGINAL_WAVEFORM = EINK_10_13C_WAVEFORM_FULL;

// Currently used waveforms
const EinkET013TT1Waveform PANEL_CLEAR_WAVEFORM = EINK_31_35C_WAVEFORM_CLEAR;
const EinkET013TT1Waveform FULL_REFRESH_WAVEFORM_REGULAR = BC_REGULAR_31_35C_WAVEFORM_FULL;
const EinkET013TT1Waveform FULL_REFRESH_WAVEFORM_INVERTED = BC_INVERTED_31_35C_WAVEFORM_FULL;
const EinkET013TT1Waveform PARTIAL_REFRESH_WAVEFORM_REGULAR =
    BC_REGULAR_EXPERIMENTAL_WAVEFORM_PARTIAL;
const EinkET013TT1Waveform PARTIAL_REFRESH_WAVEFORM_INVERTED =
    BC_INVERTED_EXPERIMENTAL_WAVEFORM_PARTIAL;
const EinkET013TT1Waveform PARTIAL_REFRESH_ORIGINAL = EINK_10_13C_WAVEFORM_PARTIAL;
const EinkET013TT1Waveform AU4_ORIGINAL = EINK_10_13C_WAVEFORM_AU4;

auto DisplayEinkET013TT1::setup() -> bool {
    DisplaySystem::setup();

    log_d("Initializing EPD driver");
    driver_.init();

    setFullWindow();

    setPartialRefresh();

    log_d("Checking Waveforms");

    uint8_t retVal = FULL_BIG_ORIGINAL_WAVEFORM.check();

    if (retVal != WAVEFORM_OK) {
        log_d("Caught a Waveform Error %d", retVal);
    }

    retVal = PARTIAL_REFRESH_WAVEFORM_REGULAR.check();

    if (retVal != WAVEFORM_OK) {
        log_d("Caught a Waveform Error %d", retVal);
    }

    return !driver_.errored();
}

// We not only need to set the bg and fg for the display,
// we also need to change the VCOM because black on white
// and white on black behave differently.
void DisplayEinkET013TT1::invertDisplay(bool newInvert) {
    bool prevInvert = invertedDisplay_;
    invertedDisplay_ = newInvert;
    if (invertedDisplay_) {
        bg = 0x0000;
        fg = 0xFFFF;
        // Only change the Vcom if the display inversion actually changes.
        if (prevInvert != invertedDisplay_) {
            log_d("Inverted to regular %d, new %d", driver_.getVcom(),
                  driver_.getVcom() + VCOM_INVERTED_MODE_DELTA);
            driver_.setVcom(driver_.getVcom() + VCOM_INVERTED_MODE_DELTA);
        }
    } else {
        bg = 0xFFFF;
        fg = 0x0000;
        // Only change the Vcom if the display inversion actually changes.
        if (prevInvert != invertedDisplay_) {
            log_d("Regular to inverted old %d, new %d", driver_.getVcom(),
                  driver_.getVcom() - VCOM_INVERTED_MODE_DELTA);
            driver_.setVcom(driver_.getVcom() - VCOM_INVERTED_MODE_DELTA);
        }
    }
    fillScreen(bg);
}

void DisplayEinkET013TT1::updateDisplay(DisplaySystem::UpdateType updateType) {
    SetCpuSpeedMhz(CPU_FREQ_MHZ_HIGH);

    switch (updateType) {
    case UNSPECIFIED:
        if (getPartialRefresh()) {
            // NOLINTBEGIN(bugprone-branch-clone) - keeping these duplicative for symmetry
            if (getInversion()) {
                if (!fullRefreshNext_) {
                    driver_.update(FULL_BIG_ORIGINAL_WAVEFORM, gfxCanvas_.getBuffer());
                } else {
                    driver_.update(FULL_BIG_ORIGINAL_WAVEFORM, gfxCanvas_.getBuffer());
                }
            } else {
                if (!fullRefreshNext_) {
                    driver_.update(FULL_BIG_ORIGINAL_WAVEFORM, gfxCanvas_.getBuffer());
                } else {
                    driver_.update(FULL_BIG_ORIGINAL_WAVEFORM, gfxCanvas_.getBuffer());
                }
            }
            // NOLINTEND(bugprone-branch-clone)
        } else {
            driver_.setBorder(bg == 0x00 ? BORDER_SETTING_BLACK : BORDER_SETTING_WHITE);
            driver_.update(FULL_REFRESH_WAVEFORM_REGULAR, gfxCanvas_.getBuffer());
        }
        break;
    case FULL:
        firstPartialTime_ = 0;
        numPartials_ = 0;
        driver_.update(FULL_BIG_ORIGINAL_WAVEFORM, gfxCanvas_.getBuffer());
        break;
    case PARTIAL:
        if (((numPartials_ > 2) && (millis() - firstPartialTime_ < 5000)) || (numPartials_ > 10) ||
            (au4Count_ > 50)) {
            fullRefreshNext_ = true;
        }
        if (!fullRefreshNext_) {
            if (firstPartialTime_ == 0) {
                firstPartialTime_ = millis();
            }
            numPartials_++;

            if (getInversion()) {
                driver_.update(PARTIAL_REFRESH_WAVEFORM_INVERTED, gfxCanvas_.getBuffer());
            } else {
                driver_.update(PARTIAL_REFRESH_WAVEFORM_REGULAR, gfxCanvas_.getBuffer());
            }
        } else {
            driver_.update(FULL_BIG_ORIGINAL_WAVEFORM, gfxCanvas_.getBuffer());
            firstPartialTime_ = 0;
            numPartials_ = 0;
            au4Count_ = 0;
        }
        break;
    case FAST:
        if (!fullRefreshNext_) {
            if (getInversion()) {
                driver_.update(BC_INVERTED_EINK_10_13C_WAVEFORM_AU4, gfxCanvas_.getBuffer());
            } else {
                driver_.update(BC_REGULAR_EINK_10_13C_WAVEFORM_AU4, gfxCanvas_.getBuffer());
            }
        } else {
            driver_.update(FULL_BIG_ORIGINAL_WAVEFORM, gfxCanvas_.getBuffer());
            firstPartialTime_ = 0;
            numPartials_ = 0;
        }
        break;
    case FAST_ORIGINAL_AU4:
        if (!fullRefreshNext_) {
            int oldVcom = getDriver()->getVcom();
            if (getInversion()) {
                getDriver()->setVcom(oldVcom - 2000);
                driver_.update(AU4_ORIGINAL, gfxCanvas_.getBuffer());
            } else {
                getDriver()->setVcom(oldVcom + 2000);
                driver_.update(AU4_ORIGINAL, gfxCanvas_.getBuffer());
            }
            getDriver()->setVcom(oldVcom);
            au4Count_++;
        } else {
            driver_.update(FULL_BIG_ORIGINAL_WAVEFORM, gfxCanvas_.getBuffer());
            firstPartialTime_ = 0;
            numPartials_ = 0;
        }
        break;
    case ORIGINAL_PARTIAL:
        if (!fullRefreshNext_) {
            driver_.update(PARTIAL_REFRESH_ORIGINAL, gfxCanvas_.getBuffer());
        } else {
            driver_.update(FULL_BIG_ORIGINAL_WAVEFORM, gfxCanvas_.getBuffer());
            firstPartialTime_ = 0;
            numPartials_ = 0;
        }
        break;
    case FAST_WITH_VCOM_NUDGE:
        if (!fullRefreshNext_) {
            int oldVcom = getDriver()->getVcom();
            if (getInversion()) {
                getDriver()->setVcom(oldVcom - 2000);
                driver_.update(BC_INVERTED_EINK_10_13C_WAVEFORM_AU4, gfxCanvas_.getBuffer());
            } else {
                getDriver()->setVcom(oldVcom + 2000);
                driver_.update(BC_REGULAR_EINK_10_13C_WAVEFORM_AU4, gfxCanvas_.getBuffer());
            }
            getDriver()->setVcom(oldVcom);
        } else {
            driver_.update(FULL_BIG_ORIGINAL_WAVEFORM, gfxCanvas_.getBuffer());
            firstPartialTime_ = 0;
            numPartials_ = 0;
        }
        break;
    }

    // Clear buffer afterwards
    fillScreen(bg);
    fullRefreshNext_ = false;
    SetCpuSpeedMhz(CPU_FREQ_MHZ_LOW);
}

#endif
