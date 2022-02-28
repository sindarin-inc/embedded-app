#if EINKET013TT1

#include "DisplayEinkET013TT1.hpp"

#include <algorithm>

#include "Displays/CustomWaveforms.hpp"
#include "Displays/EinkWaveforms.hpp"
#include "Misc/Platform.hpp"
#include "config.h"

#define EinkET013TT1_WIDTH_IN_MM 23.3
#define EinkET013TT1_HEIGHT_IN_MM 23.3

// Not currently used waveforms
const EinkET013TT1Waveform FULL_REFRESH_WAVEFORM = BC_EINK_31_35C_WAVEFORM_FULL;
const EinkET013TT1Waveform ORIGINAL_FULL_REFRESH_WAVEFORM = EINK_ORIGINAL_WAVEFORM_FULL;
const EinkET013TT1Waveform ORIGINAL_PARTIAL_REFRESH_WAVEFORM = EINK_31_35C_WAVEFORM_PARTIAL;

// Currently used waveforms
const EinkET013TT1Waveform PANEL_CLEAR_WAVEFORM = EINK_31_35C_WAVEFORM_CLEAR;
const EinkET013TT1Waveform FULL_REFRESH_WAVEFORM_REGULAR = BC_REGULAR_31_35C_WAVEFORM_FULL;
const EinkET013TT1Waveform FULL_REFRESH_WAVEFORM_INVERTED = BC_INVERTED_31_35C_WAVEFORM_FULL;
const EinkET013TT1Waveform PARTIAL_REFRESH_WAVEFORM_REGULAR =
    BC_REGULAR_EXPERIMENTAL_WAVEFORM_PARTIAL;
const EinkET013TT1Waveform PARTIAL_REFRESH_WAVEFORM_INVERTED =
    BC_INVERTED_EXPERIMENTAL_WAVEFORM_PARTIAL;

void DisplayEinkET013TT1::setup() {
    log_d("Initializing EPD driver");
    driver_.init();
    log_d("Clearing display");
    driver_.clear(PANEL_CLEAR_WAVEFORM);

    setFullWindow();

    widthInMm_ = EinkET013TT1_WIDTH_IN_MM;
    heightInMm_ = EinkET013TT1_HEIGHT_IN_MM;

    setPartialRefresh();

    log_i("Display ready!");
}

void DisplayEinkET013TT1::updateDisplay() {
    SetCpuSpeedMhz(CPU_FREQ_MHZ_HIGH);

    if (getPartialRefresh()) {
        if (getInversion()) {
            if (!fullRefreshNext_) {
                driver_.update(PARTIAL_REFRESH_WAVEFORM_INVERTED, gfxClass()->getBuffer());
            } else {
                driver_.update(FULL_REFRESH_WAVEFORM_INVERTED, gfxClass()->getBuffer());
            }
        } else {
            if (!fullRefreshNext_) {
                driver_.update(PARTIAL_REFRESH_WAVEFORM_REGULAR, gfxClass()->getBuffer());
            } else {
                driver_.update(FULL_REFRESH_WAVEFORM_REGULAR, gfxClass()->getBuffer());
            }
        }
    } else {
        driver_.setBorder(bg == 0x00 ? BORDER_SETTING_BLACK : BORDER_SETTING_WHITE);
        driver_.update(FULL_REFRESH_WAVEFORM_REGULAR, gfxClass()->getBuffer());
    }
    // Clear buffer afterwards
    fillScreen(bg);
    fullRefreshNext_ = false;
    SetCpuSpeedMhz(CPU_FREQ_MHZ_LOW);
}

#endif
