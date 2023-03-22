#include <stdbool.h>

#include "templeLookup.h"

bool checkTable(uint8_t x, uint8_t z) {
    // truncate values
    int entryIndex = (x * 2);
    int16_t testVal = (int16_t)z;

    if (templeAngleValues[entryIndex] < 0) {
        return false;
    }

    if (testVal >= templeAngleValues[entryIndex] && testVal < templeAngleValues[entryIndex + 1]) {
        return true;
    }
    return false;
}
