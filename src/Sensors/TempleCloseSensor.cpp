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
    // Don't fire the callback if this is the first call to this method
    // (because we can't determine yet if this is a state change)
    bool fireCallback = opened_.has_value() && templeOpen != opened_.value() && callback_;
    if (fireCallback) {
        log_i("Temple is %s", templeOpen ? "open" : "closed");
        callback_(templeOpen);
    }
    opened_ = templeOpen;
}
