#if EINKET013TT1

#include "DisplayEinkET013TT1.hpp"

#include <algorithm>

#include "Displays/CustomWaveforms.hpp"
#include "Displays/EinkWaveforms.hpp"

#define EinkET013TT1_WIDTH_IN_MM 23.3
#define EinkET013TT1_HEIGHT_IN_MM 23.3
const EinkET013TT1Waveform PANEL_CLEAR_WAVEFORM = EINK_31_35C_WAVEFORM_CLEAR;
const EinkET013TT1Waveform FULL_REFRESH_WAVEFORM = BC_EINK_31_35C_WAVEFORM_FULL;
const EinkET013TT1Waveform PARTIAL_REFRESH_WAVEFORM_REGULAR =
    BC_REGULAR_EXPERIMENTAL_WAVEFORM_PARTIAL;
const EinkET013TT1Waveform PARTIAL_REFRESH_WAVEFORM_INVERTED =
    BC_INVERTED_EXPERIMENTAL_WAVEFORM_PARTIAL;
const EinkET013TT1Waveform ORIGINAL_FULL_REFRESH_WAVEFORM = EINK_ORIGINAL_WAVEFORM_FULL;
const EinkET013TT1Waveform ORIGINAL_PARTIAL_REFRESH_WAVEFORM = EINK_31_35C_WAVEFORM_PARTIAL;

DisplayEinkET013TT1::DisplayEinkET013TT1(uint8_t cs, uint8_t oei, uint8_t rst, uint8_t busy)
    : driver_(cs, oei, rst, busy), DisplaySystem(EinkET013TT1_X_PIXELS, EinkET013TT1_Y_PIXELS) {
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
    if (getPartialRefresh()) {
        if (getInversion()) {
            driver_.update(PARTIAL_REFRESH_WAVEFORM_INVERTED, getBuffer());
        } else {
            driver_.update(PARTIAL_REFRESH_WAVEFORM_REGULAR, getBuffer());
        }
    } else {
        driver_.setBorder(bg == 0x00 ? BORDER_SETTING_BLACK : BORDER_SETTING_WHITE);
        driver_.update(FULL_REFRESH_WAVEFORM, getBuffer());
    }
    // Clear buffer afterwards
    fillScreen(bg);
}

void DisplayEinkET013TT1::clearDisplay() { driver_.clear(PANEL_CLEAR_WAVEFORM); }
#endif
