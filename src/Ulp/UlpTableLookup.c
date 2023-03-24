#include <stdbool.h>

#include "TempleLookup.h"

bool checkTable(uint8_t x, uint8_t z) {
    int entryIndex = (x * 2);
    uint8_t start = templeAngleValues[entryIndex];
    uint8_t end = templeAngleValues[entryIndex + 1];

    // check for special cases
    if (end == 0) {
        if (start == 0) {
            // temple is not open at all for this value of x
            return false;
        }

        if (z >= start) {
            // after start value, all z values have the temple open
            return true;
        }

        return false;
    }

    if (z >= start && z < end) {
        return true;
    }
    return false;
}
