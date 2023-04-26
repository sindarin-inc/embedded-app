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

auto TempleCloseSensor::isTempleOpen(bool &error) -> bool {
    error = false;
    Xyz8bit magReading;
    auto success = magSensor_.readXZ8Bit(magReading);
    if (!success) {
        log_e("Failed to read magnetometer");
        error = true;
        return false;
    }
    return IsTempleOpen(magReading.x, magReading.z);
}

auto TempleCloseSensor::checkTemple() -> bool {
    bool error = false;
    auto templeOpen = isTempleOpen(error);

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
    if (error) {
        log_e("failed to read magnetometer");
        return false;
    }

    return true;
}
