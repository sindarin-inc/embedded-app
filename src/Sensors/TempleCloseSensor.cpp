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

auto TempleCloseSensor::isTempleOpen(bool &success) -> bool {
    Xyz8bit magReading;
    success = magSensor_.readXZ8Bit(magReading);
    if (!success) {
        log_e("Failed to read magnetometer");
        return false;
    }
    return IsTempleOpen(magReading.x, magReading.z);
}

void TempleCloseSensor::checkTemple() {
    bool success = true;
    bool templeOpen = isTempleOpen(success);

    if (!success) {
        log_i("checkTemple failed");
        return;
    }

    // Don't fire the callback if this is the first call to this method
    // (because we can't determine yet if this is a state change)
    bool stateChanged = opened_.has_value() && templeOpen != opened_.value();
    if (stateChanged) {
        log_i("Temple is %s", templeOpen ? "open" : "closed");
        if (callback_) {
            callback_(templeOpen);
        }
    }
    opened_ = templeOpen;
}
