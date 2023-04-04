#include <cinttypes>
#include <cmath>
#include <cstdio>

#include "../../src/Sensors/TempleAngle.hpp"

// generate a table to decide if the temple is open given all possible
// values of x and z from the magnetometer.
// Fortunately this is a smooth function (I think that's the name)
// and we can condense the table by storing just two values per value of x:
// the start and end value of z where the temple is open.
// For the special cases where the temple is open all the way to the highest value
// of z, make the 'end' byte 0.
// If the temple is not open at all for a given row, both bytes are 0.

auto main(int argc, char **argv) -> int {
    int size = 256 * 2;
    printf("#include <stdint.h>\n");
    printf("const uint8_t templeAngleValues[%d] = {\n", size);

    int bit = 0;
    uint8_t byte = 0x00;
    int byteIndex = 0;
    for (int x = 0; x < 256; x++) {
        bool startFound = false;
        bool endFound = false;

        uint8_t openStart = 0;
        uint8_t openEnd = 0;
        for (int z = 0; z < 256; z++) {
            int xval = (int)x - 128;
            int zval = (int)z - 128;

            bool open = IsTempleOpen(xval, zval);

            if (open) {
                if (!startFound) {
                    startFound = true;
                    openStart = z;
                }
            } else { // temple is closed
                if (startFound && !endFound) {
                    endFound = true;
                    openEnd = z;
                }
            }
        }
        printf("%" PRIu8 ",%" PRIu8, openStart, openEnd);
        if (x < 255) {
            printf(",");
        }
        printf("\n");
    }
    printf("};\n");
}