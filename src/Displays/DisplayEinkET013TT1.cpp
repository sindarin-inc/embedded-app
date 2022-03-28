#if EINKET013TT1

#include "DisplayEinkET013TT1.hpp"

#include <algorithm>

#include "Displays/CustomWaveforms.hpp"
#include "Displays/EinkWaveforms.hpp"
#include "Misc/Platform.hpp"
#include "config.h"

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

auto DisplayEinkET013TT1::setup() -> bool {
    DisplaySystem::setup();

    log_d("Initializing EPD driver");
    driver_.init();
    log_d("Clearing display");
    driver_.clear(PANEL_CLEAR_WAVEFORM);

    setFullWindow();

    setPartialRefresh();

    log_d("Checking Waveforms");

    uint8_t retVal = driver_.checkWaveform(FULL_BIG_ORIGINAL_WAVEFORM);

    if (retVal != WAVEFORM_OK) {
        log_d("Caught a Waveform Error %d", retVal);
    }

    retVal = driver_.checkWaveform(PARTIAL_REFRESH_WAVEFORM_REGULAR);

    if (retVal != WAVEFORM_OK) {
        log_d("Caught a Waveform Error %d", retVal);
    }

    log_i("Display ready!");

    return !driver_.errored();
}

void DisplayEinkET013TT1::updateDisplay(DisplaySystem::UpdateType updateType) {
    SetCpuSpeedMhz(CPU_FREQ_MHZ_HIGH);

    switch (updateType) {
    case UNSPECIFIED:
        if (getPartialRefresh()) {
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
        } else {
            driver_.setBorder(bg == 0x00 ? BORDER_SETTING_BLACK : BORDER_SETTING_WHITE);
            driver_.update(FULL_REFRESH_WAVEFORM_REGULAR, gfxCanvas_.getBuffer());
        }
        break;
    case FULL:
        driver_.update(FULL_BIG_ORIGINAL_WAVEFORM, gfxCanvas_.getBuffer());
        break;
    case PARTIAL:
        driver_.update(PARTIAL_REFRESH_WAVEFORM_REGULAR, gfxCanvas_.getBuffer());
        break;
    case FAST:
        driver_.update(EINK_10_13C_WAVEFORM_AU4, gfxCanvas_.getBuffer());
        break;
    }

    // Clear buffer afterwards
    fillScreen(bg);
    fullRefreshNext_ = false;
    SetCpuSpeedMhz(CPU_FREQ_MHZ_LOW);
}

#endif
