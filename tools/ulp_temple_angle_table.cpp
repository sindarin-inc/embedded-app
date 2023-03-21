#include <stdio.h>
#include <math.h>

#include "../src/Sensors/TempleAngle.hpp"

int main(int argc, char** argv) {
    int divisor = 2;
    int size = ((256/divisor) * (256/divisor)) / 8;

    printf("#include <stdint.h>\n");
    printf("#define TABLE_MAG_DIVISOR %d\n", divisor);
    printf("#define TABLE_ROW_SIZE %d\n", 256/divisor);
    printf("const uint8_t temple_angle_values[%d] = {\n", size);

    int bit = 0;
    uint8_t byte = 0x00;
    int byteIndex = 0;
    for (int x = 0; x < 256; x += divisor)
    {
        for (int z = 0; z < 256; z += divisor)
        {
            int xval = (int)x - 128;
            int zval = (int)z - 128;

            bool open = IsTempleOpen(xval, zval);
            if (open) {
                uint8_t bitmask = 0x01 << bit;
                byte |= bitmask;
            }
            bit++;
            if (bit == 8) {
                byteIndex++;
                printf("0x%02X", byte);
                if (byteIndex < size) {
                    printf(",");
                }
                bit = 0;
                byte = 0;
            }
        }
        printf("\n");
    }
    printf("};\n");
}