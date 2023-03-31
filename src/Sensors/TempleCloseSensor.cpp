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
    if (templeOpen && opened_.value_or(false) == false) {
        log_i("Temple is open");
        if (callback_) {
            callback_(true);
        }
        opened_ = true;
    } else if (!templeOpen && opened_.value_or(false) == true) {
        log_i("Temple is closed");
        if (callback_) {
            callback_(false);
        }
        opened_ = false;
    }
}
