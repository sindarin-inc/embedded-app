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

auto TempleCloseSensor::isTempleOpen() -> bool {
    Xyz8bit magReading;
    auto success = magSensor_.readXZ8Bit(magReading);
    if (!success) {
        log_e("Failed to read magnetometer");
        return false;
    }
    return IsTempleOpen(magReading.x, magReading.z);
}

void TempleCloseSensor::checkTemple() {
    auto templeOpen = isTempleOpen();
    bool templeWasOpen = opened_.value_or(false);
    if (templeOpen != templeWasOpen) {
        if (templeOpen) {
            log_i("Temple is open");
        } else {
            log_i("Temple is closed");
        }
        if (opened_.has_value() && callback_) {
            callback_(templeOpen);
        }
    }
    opened_ = templeOpen;
}
