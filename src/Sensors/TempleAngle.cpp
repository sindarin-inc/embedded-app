#include "TempleAngle.hpp"

#include <cmath>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "sindarin-debug.h"

auto IsTempleOpen(int8_t x, int8_t z, float *angleOut) -> bool {
    // There is a pretty spectacular bug here where this math sometimes does stuff like this:
    // D (19244) Sol: Mag: x=-2 z=12 -9.462322º
    // D (20434) Sol: Mag: x=-2 z=12 -783158239771330399125549088768.000000º
    // D (22022) Sol: Mag: x=-2 z=12 -9.462322º
    // D (32113) Sol: Mag: x=-2 z=12 -9.462322º
    // D (33536) Sol: Mag: x=-2 z=12 -infº
    // D (34842) Sol: Mag: x=-2 z=12 -9.462322º
    // It only seems to happen when downloading files, and disabling interrupts seems to fix it.
    // Looks like it's probably this: https://github.com/espressif/esp-idf/issues/11690 and is
    // supposedly fixed in IDF 5.1.1
    float angle = atan2f(x, z) * 180.0f / M_PI;
    // log_d("Mag: x=%d z=%d %fº", x, z, angle);
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
    if (angleOut != nullptr) {
        *angleOut = angle;
    }
    // Define an angle, above which we will declare the temple as open and below which we will
    // declare it as closed. This value was discovered experimentally.
    constexpr int templeMidpointAngle = 70;
    if (angle > templeMidpointAngle) {
        return true;
    }

    return false;
}