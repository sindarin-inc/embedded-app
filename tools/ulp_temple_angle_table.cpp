#include <stdio.h>
#include <math.h>
#include <inttypes.h>

#include "../src/Sensors/TempleAngle.hpp"

int main(int argc, char** argv) {
    int size = 256*2;
    printf("#include <stdint.h>\n");
    printf("const int16_t templeAngleValues[%d] = {\n", size);

    int bit = 0;
    uint8_t byte = 0x00;
    int byteIndex = 0;
    for (int x = 0; x < 256; x++)
    {
        int16_t openStart = -1;
        int16_t openEnd = 256;
        for (int z = 0; z < 256; z++)
        {
            int xval = (int)x - 128;
            int zval = (int)z - 128;

            bool open = IsTempleOpen(xval, zval);

            if (open) {
                if (openStart < 0) {
                    openStart = z;
                }
            } else {
                if (openStart >= 0 && openEnd > 255) {
                    openEnd = z;
                }
            }
        }
        printf("%" PRIi16 ",%" PRIi16 ",\n", openStart, openEnd);
    }
    printf("};\n");
}