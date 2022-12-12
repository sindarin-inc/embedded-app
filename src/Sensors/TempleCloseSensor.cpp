#include "TempleCloseSensor.hpp"

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

void TempleCloseSensor::checkTemple() {
    Xyz8bit magReading;
    auto success = magSensor_.readXZ8Bit(magReading);
    if (!success) {
        log_e("Failed to read magnetometer");
        return;
    }
    float angle = atan2f(magReading.x, magReading.z) * 180.0f / M_PI;
    // log_d("Mag: %d %d %d %fº (success=%d)", magReading.x, magReading.y, magReading.z, angle,
    //       success);
    // Experimentally I've seen the following ranges:
    // -17º (closed) to 104º (open) (midpoint = 43.5º)
    // -150º (closed) to -73º (open) (midpoint = -112º).
    // Handle either magnet polarity:
    if (angle < -45) {
        angle += 180;
    }
    // Define an angle, above which we will declare the temple as open and below which we will
    // declare it as closed. This value was discovered experimentally.
    constexpr int templeMidpointAngle = 70;
    if (angle > templeMidpointAngle && opened_ == false) {
        log_i("Temple is open");
        opened_ = true;
        if (callback_) {
            callback_(true);
        }
    } else if (angle < templeMidpointAngle && opened_ == true) {
        log_i("Temple is closed");
        opened_ = false;
        if (callback_) {
            callback_(false);
        }
    }
}
