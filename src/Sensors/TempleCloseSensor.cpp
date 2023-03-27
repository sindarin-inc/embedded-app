#include "TempleCloseSensor.hpp"

#include <cmath>

#include "TempleAngle.hpp"
#include "sindarin-debug.h"

auto TempleCloseSensor::checkProductId() -> bool {
    auto mmcProductId = magSensor_.readProductId();
    if (mmcProductId != 0x10) {
        log_e("MMC5603 magnetometer not found (productId: 0x%02X)", mmcProductId);
        return false;
    } else {
        log_i("MMC5603 magnetometer found (productId: 0x%02X)", mmcProductId);
        return true;
    }
}

void TempleCloseSensor::resetLongCloseTimer() {
    if (longCloseTimer_) {
        xTimerDelete(longCloseTimer_, 0);
        longCloseTimer_ = nullptr;
    }
}

void TempleCloseSensor::checkTemple() {
    Xyz8bit magReading;
    auto success = magSensor_.readXZ8Bit(magReading);
    if (!success) {
        log_e("Failed to read magnetometer");
        return;
    }
    bool templeOpen = IsTempleOpen(magReading.x, magReading.z);

    if (templeOpen && opened_ == false) {
        log_i("Temple is open");
        opened_ = true;
        if (callback_) {
            callback_(true);
        }
        resetLongCloseTimer();
    } else if (!templeOpen && opened_ == true) {
        log_i("Temple is closed");
        opened_ = false;
        if (callback_) {
            callback_(false);
        }

        if (longCloseCallback_) {
            // Start a timer to fire after 60s of being closed
            if (longCloseTimer_) {
                log_w("Somehow tried to create a new longCloseTimer_ while one was already active");
                resetLongCloseTimer();
            }

            auto onTimer = [](TimerHandle_t timer) {
                auto *templeCloseSensor =
                    static_cast<TempleCloseSensor *>(pvTimerGetTimerID(timer));
                templeCloseSensor->resetLongCloseTimer();
                templeCloseSensor->longCloseCallback_();
            };

            longCloseTimer_ =
                xTimerCreate("LongCloseTimer", pdMS_TO_TICKS(15000), pdFALSE, this, onTimer);
            xTimerStart(longCloseTimer_, 0);
        }
    }
}
