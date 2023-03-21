#include "table.h"
#include <stdio.h>
#include "../src/Sensors/TempleAngle.hpp"

#define TABLE_MAG_DIVISOR 2

bool checkTable(uint8_t x, uint8_t z) {
    // truncate values
    uint8_t xx = x / TABLE_MAG_DIVISOR;
    uint8_t zz = z / TABLE_MAG_DIVISOR;
    int rowSize = 256 / TABLE_MAG_DIVISOR;
    
    int entryIndex = (xx * rowSize) + zz;
    int byteIndex = entryIndex / 8;
    int bitIndex = entryIndex % 8;

    uint8_t tableByte = temple_angle_values[byteIndex];
    uint8_t mask = 0x01 << bitIndex;

    if ((tableByte & mask) != 0) {
        return true;
    }

    return false;
}

bool checkFunc(uint8_t x, uint8_t z) {

    int xxx = (x/2)*2;
    int zzz = (z/2)*2;

    int xx = (int)xxx - 128;
    int zz = (int)zzz - 128;

    return IsTempleOpen(xx, zz);
}

int main(int argc, char** argv) {
    // test every possible value of x and z mag values
    // check if the table works

    int errors = 0;
    for (int x = 0; x < 256; x++) {
        for (int z = 0; z < 256; z++) {
            bool t = checkTable(x, z);
            bool f = checkFunc(x, z);

            if (t != f) {
                printf("mismatch at x: %d z %d: %d %d\n", x, z, t, f);
                errors++;
            }
        }
    }

    printf("finished, %d errors\n", errors);
}