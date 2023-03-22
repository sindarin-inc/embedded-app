#include "TempleAngle.hpp"

#include <cmath>

auto IsTempleOpen(int8_t x, int8_t z) -> bool {
    float angle = atan2f(x, z) * 180.0f / M_PI;
    // log_d("Mag: %d %d %d %fº (success=%d)", magReading.x, magReading.y, magReading.z, angle,
    //       success);
    // Experimentally I've seen the following ranges:
    // -17º (closed) to 104º (open) (midpoint = 43.5º)
    // -150º (closed) to -73º (open) (midpoint = -112º)
    // -197 (closed) to -86 (open) (midpoint = -141.5º) - SOL01224400004
    // Handle either magnet polarity:
    if (angle < -45) {
        angle += 180;
    } else if (angle > 135) {
        angle -= 180;
    }
    // Define an angle, above which we will declare the temple as open and below which we will
    // declare it as closed. This value was discovered experimentally.
    constexpr int templeMidpointAngle = 70;
    if (angle > templeMidpointAngle) {
        return true;
    }

    return false;
}