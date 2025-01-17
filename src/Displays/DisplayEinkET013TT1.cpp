#if CONFIG_DISPLAY_EINKET013TT1

#include "DisplayEinkET013TT1.hpp"

#include <algorithm>
#include <freertos/task.h>

#include "Communications/USBSerial.hpp"
#include "Displays/CustomWaveforms.hpp"
#include "Displays/EinkWaveforms.hpp"
#include "Misc/Dispatch.hpp"
#include "Misc/Platform.hpp"
#include "config.h"

#define VCOM_INVERTED_MODE_DELTA 2500

// Not currently used waveforms
// const EinkET013TT1Waveform FULL_REFRESH_WAVEFORM = BC_EINK_31_35C_WAVEFORM_FULL;
// const EinkET013TT1Waveform ORIGINAL_FULL_REFRESH_WAVEFORM = EINK_ORIGINAL_WAVEFORM_FULL;
// const EinkET013TT1Waveform ORIGINAL_PARTIAL_REFRESH_WAVEFORM = EINK_31_35C_WAVEFORM_PARTIAL;

const EinkET013TT1Waveform FULL_BIG_ORIGINAL_WAVEFORM = EINK_10_13C_WAVEFORM_FULL;

// Currently used waveforms
const EinkET013TT1Waveform ERASE_WAVEFORM_REGULAR = BC_REGULAR_EINK_10_13C_WAVEFORM_ERASE;
const EinkET013TT1Waveform ERASE_WAVEFORM_INVERTED = BC_REGULAR_EINK_10_13C_WAVEFORM_ERASE;
// const EinkET013TT1Waveform PANEL_CLEAR_WAVEFORM = EINK_31_35C_WAVEFORM_CLEAR;
const EinkET013TT1Waveform FULL_REFRESH_WAVEFORM_REGULAR = BC_REGULAR_31_35C_WAVEFORM_FULL;
// const EinkET013TT1Waveform FULL_REFRESH_WAVEFORM_INVERTED = BC_INVERTED_31_35C_WAVEFORM_FULL;
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
            log_d("Regular to dark mode, old %d, new %d", driver_.getVcom(),
                  driver_.getVcom() + VCOM_INVERTED_MODE_DELTA);
            driver_.setVcomNudge(VCOM_INVERTED_MODE_DELTA);
        }
    } else {
        bg = 0xFFFF;
        fg = 0x0000;
        // Only change the Vcom if the display inversion actually changes.
        if (prevInvert != invertedDisplay_) {
            log_d("Dark mode to regular, old %d, new %d", driver_.getVcom(),
                  driver_.getVcom() - VCOM_INVERTED_MODE_DELTA);
            driver_.setVcomNudge(0);
        }
    }
    fillScreen(bg);
}

void DisplayEinkET013TT1::updateDisplay(DisplaySystem::UpdateType updateType, bool clearFrameBuffer,
                                        int vcomNudge, DisplaySystem::UpdateType altUpdateType,
                                        ScreenSide screen, SolRect altRect) {
    auto queue = DispatchGetUiQueue(false);
    if (mirrorToSerial) {
        sendBufferToSerial(gfxCanvas_.getBuffer());
    }
    if (queue == nullptr || !queue->isInQueue()) {
        log_w("updateDisplay called from outside the UI queue.");
    }
    int adjustedVcom = driver_.getVcom() + vcomNudge;
    switch (updateType) {
    case UNSPECIFIED:
        if (getPartialRefresh()) {
            // NOLINTBEGIN(bugprone-branch-clone) - keeping these duplicative for symmetry
            if (getInversion()) {
                if (!fullRefreshNext_) {
                    driver_.update(FULL_BIG_ORIGINAL_WAVEFORM, gfxCanvas_.getBuffer(), adjustedVcom,
                                   BC_WAVEFORM_NONE, screen);
                } else {
                    driver_.update(FULL_BIG_ORIGINAL_WAVEFORM, gfxCanvas_.getBuffer(), adjustedVcom,
                                   BC_WAVEFORM_NONE, screen);
                }
            } else {
                if (!fullRefreshNext_) {
                    driver_.update(FULL_BIG_ORIGINAL_WAVEFORM, gfxCanvas_.getBuffer(), adjustedVcom,
                                   BC_WAVEFORM_NONE, screen);
                } else {
                    driver_.update(FULL_BIG_ORIGINAL_WAVEFORM, gfxCanvas_.getBuffer(), adjustedVcom,
                                   BC_WAVEFORM_NONE, screen);
                }
            }
            // NOLINTEND(bugprone-branch-clone)
        } else {
            driver_.setBorder(bg == 0x00 ? BORDER_SETTING_BLACK : BORDER_SETTING_WHITE);
            driver_.update(FULL_REFRESH_WAVEFORM_REGULAR, gfxCanvas_.getBuffer(), adjustedVcom,
                           BC_WAVEFORM_NONE, screen);
        }
        break;
    case FULL:
        firstPartialTime_ = 0;
        numPartials_ = 0;
        if (altUpdateType != UNSPECIFIED) {
            switch (altUpdateType) {
            // right now these are the only two supported
            case FAST:
                driver_.update(FULL_BIG_ORIGINAL_WAVEFORM, gfxCanvas_.getBuffer(), adjustedVcom,
                               BC_REGULAR_EINK_10_13C_WAVEFORM_AU4, screen,
                               {altRect.origin.x, altRect.origin.x + altRect.size.width,
                                altRect.origin.y, altRect.origin.y + altRect.size.height});
                break;
            case ERASE:
                driver_.update(FULL_BIG_ORIGINAL_WAVEFORM, gfxCanvas_.getBuffer(), adjustedVcom,
                               ERASE_WAVEFORM_REGULAR, screen,
                               {altRect.origin.x, altRect.origin.x + altRect.size.width,
                                altRect.origin.y, altRect.origin.y + altRect.size.height});
                break;
            default:
                log_e("UNSUPPORTED ALT UPDATE TYPE");
                break;
            }
        } else {
            driver_.update(FULL_BIG_ORIGINAL_WAVEFORM, gfxCanvas_.getBuffer(), adjustedVcom,
                           BC_WAVEFORM_NONE, screen);
        }
        break;
    case PARTIAL:
        if (!forceRefreshType_ && (((numPartials_ > 2) && (Millis() - firstPartialTime_ < 5000)) ||
                                   (numPartials_ > 10) || (au4Count_ > 50))) {
            fullRefreshNext_ = true;
        }
        if (!fullRefreshNext_) {
            if (firstPartialTime_ == 0) {
                firstPartialTime_ = Millis();
            }
            numPartials_++;

            if (getInversion()) {
                driver_.update(PARTIAL_REFRESH_WAVEFORM_INVERTED, gfxCanvas_.getBuffer(),
                               adjustedVcom, BC_WAVEFORM_NONE, screen);
            } else {
                if (altUpdateType != UNSPECIFIED) {
                    switch (altUpdateType) {
                    // right now these are the only two supported
                    case FAST:
                        driver_.update(PARTIAL_REFRESH_WAVEFORM_REGULAR, gfxCanvas_.getBuffer(),
                                       adjustedVcom, BC_REGULAR_EINK_10_13C_WAVEFORM_AU4, screen,
                                       {altRect.origin.x, altRect.origin.x + altRect.size.width,
                                        altRect.origin.y, altRect.origin.y + altRect.size.height});
                        break;
                    case ERASE:
                        driver_.update(PARTIAL_REFRESH_WAVEFORM_REGULAR, gfxCanvas_.getBuffer(),
                                       adjustedVcom, ERASE_WAVEFORM_REGULAR, screen,
                                       {altRect.origin.x, altRect.origin.x + altRect.size.width,
                                        altRect.origin.y, altRect.origin.y + altRect.size.height});
                        break;
                    default:
                        log_e("UNSUPPORTED ALT UPDATE TYPE");
                        break;
                    }
                } else {
                    driver_.update(PARTIAL_REFRESH_WAVEFORM_REGULAR, gfxCanvas_.getBuffer(),
                                   adjustedVcom, BC_WAVEFORM_NONE, screen);
                }
            }
        } else {
            driver_.update(FULL_BIG_ORIGINAL_WAVEFORM, gfxCanvas_.getBuffer(), adjustedVcom,
                           BC_WAVEFORM_NONE, screen);
            firstPartialTime_ = 0;
            numPartials_ = 0;
            au4Count_ = 0;
        }
        break;
    case FAST:
        if (!fullRefreshNext_) {
            if (getInversion()) {
                driver_.update(BC_INVERTED_EINK_10_13C_WAVEFORM_AU4, gfxCanvas_.getBuffer(),
                               adjustedVcom, BC_WAVEFORM_NONE, screen);
            } else {
                driver_.update(BC_REGULAR_EINK_10_13C_WAVEFORM_AU4, gfxCanvas_.getBuffer(),
                               adjustedVcom, BC_WAVEFORM_NONE, screen);
            }
        } else {
            driver_.update(FULL_BIG_ORIGINAL_WAVEFORM, gfxCanvas_.getBuffer(), adjustedVcom,
                           BC_WAVEFORM_NONE, screen);
            firstPartialTime_ = 0;
            numPartials_ = 0;
        }
        break;
    case ERASE:
        if (getInversion()) {
            if (altUpdateType != UNSPECIFIED) {
                switch (altUpdateType) {
                // right now these are the only two supported
                case FAST:
                    driver_.update(ERASE_WAVEFORM_REGULAR, gfxCanvas_.getBuffer(), adjustedVcom,
                                   BC_REGULAR_EINK_10_13C_WAVEFORM_AU4, screen,
                                   {altRect.origin.x, altRect.origin.x + altRect.size.width,
                                    altRect.origin.y, altRect.origin.y + altRect.size.height});
                    break;
                case ERASE:
                    driver_.update(ERASE_WAVEFORM_REGULAR, gfxCanvas_.getBuffer(), adjustedVcom,
                                   ERASE_WAVEFORM_REGULAR, screen,
                                   {altRect.origin.x, altRect.origin.x + altRect.size.width,
                                    altRect.origin.y, altRect.origin.y + altRect.size.height});
                    break;
                default:
                    log_e("UNSUPPORTED ALT UPDATE TYPE");
                    break;
                }
            } else {
                driver_.update(ERASE_WAVEFORM_REGULAR, gfxCanvas_.getBuffer(), adjustedVcom,
                               BC_WAVEFORM_NONE, screen);
            }
        } else {
            driver_.update(ERASE_WAVEFORM_INVERTED, gfxCanvas_.getBuffer(), adjustedVcom,
                           BC_WAVEFORM_NONE, screen);
        }
        numPartials_++;
        break;
    case FAST_ORIGINAL_AU4:
        if (!fullRefreshNext_) {
            if (getInversion()) {
                driver_.update(AU4_ORIGINAL, gfxCanvas_.getBuffer(), adjustedVcom - 2000,
                               BC_WAVEFORM_NONE, screen);
            } else {
                driver_.update(AU4_ORIGINAL, gfxCanvas_.getBuffer(), adjustedVcom + 2000,
                               BC_WAVEFORM_NONE, screen);
            }
            au4Count_++;
        } else {
            driver_.update(FULL_BIG_ORIGINAL_WAVEFORM, gfxCanvas_.getBuffer(), adjustedVcom,
                           BC_WAVEFORM_NONE, screen);
            firstPartialTime_ = 0;
            numPartials_ = 0;
        }
        break;
    case ORIGINAL_PARTIAL:
        if (!fullRefreshNext_) {
            driver_.update(PARTIAL_REFRESH_ORIGINAL, gfxCanvas_.getBuffer(), 0, BC_WAVEFORM_NONE,
                           screen);
        } else {
            driver_.update(FULL_BIG_ORIGINAL_WAVEFORM, gfxCanvas_.getBuffer(), 0, BC_WAVEFORM_NONE,
                           screen);
            firstPartialTime_ = 0;
            numPartials_ = 0;
        }
        break;
    case FAST_INDICATOR:
        if (!fullRefreshNext_) {
            if (getInversion()) {
                driver_.update(BC_INVERTED_EINK_10_13C_WAVEFORM_AU4, gfxCanvas_.getBuffer(),
                               adjustedVcom - 2000, BC_WAVEFORM_NONE, screen);
            } else {
                driver_.update(BC_REGULAR_EINK_10_13C_WAVEFORM_AU4, gfxCanvas_.getBuffer(),
                               adjustedVcom + 2000, BC_WAVEFORM_NONE, screen);
            }
        } else {
            driver_.update(FULL_BIG_ORIGINAL_WAVEFORM, gfxCanvas_.getBuffer(), adjustedVcom,
                           BC_WAVEFORM_NONE, screen);
            firstPartialTime_ = 0;
            numPartials_ = 0;
        }
        break;
    }

    // Clear buffer afterwards if argument not set to false.
    if (clearFrameBuffer) {
        fillScreen(bg);
    }

    fullRefreshNext_ = false;

    // Uncomment for some TTF performance debugging
    // #if CONFIG_FONT_TTF
    //     font_->getFontData()->cache.showStats();
    //     log_d("Free stack space: %u", uxTaskGetStackHighWaterMark(nullptr));
    // #endif
}

void DisplayEinkET013TT1::drawFullscreenBitmapFromFile(RendererStream &stream) {
    // data is packed 1 bit binary
    uint8_t *buffer = gfxCanvas_.getBuffer();
    assert(buffer);
    int size = gfxCanvas_.width() * gfxCanvas_.height() / 8;

    if (stream.readBytes(buffer, size) != size) {
        log_e("failed to read pbm bytes");
        return;
    }

    // pbm files have 0 white and 1 black
    // invert to display properly (for now)
    for (int i = 0; i < size; i++) {
        buffer[i] = buffer[i] ^ 0xFF; // XOR to invert all bits
    }
}

void DisplayEinkET013TT1::sendBufferToSerial(const uint8_t *buffer, bool screenshot) const {
    constexpr size_t byteSize = EinkET013TT1_PIXEL_WIDTH * EinkET013TT1_PIXEL_HEIGHT / 8;

    std::string *prefix;
    if (screenshot) {
        static std::string screenshotPrefix;
        if (screenshotPrefix.empty()) {
            screenshotPrefix = StringFormat("SCREENSHOT %zu", byteSize);
        }
        prefix = &screenshotPrefix;
    } else {
        static std::string framePrefix;
        if (framePrefix.empty()) {
            framePrefix = StringFormat("FRAME %zu", byteSize);
        }
        prefix = &framePrefix;
    }
    USBSerial::getInstance().txSendLine(*prefix);
    USBSerial::getInstance().txSendData(buffer, byteSize);
    USBSerial::getInstance().txSendLine("");
}

void DisplayEinkET013TT1::sendCurrentScreenToSerial(bool screenshot) const {
    auto buffer = driver_.getBuffer();
    sendBufferToSerial(buffer, screenshot);
};

#endif
