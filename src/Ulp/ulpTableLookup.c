#include "templeLookup.h"

bool checkTable(uint8_t x, uint8_t z) {
    // truncate values
    uint8_t xx = x / TABLE_MAG_DIVISOR;
    uint8_t zz = z / TABLE_MAG_DIVISOR;
    
    int entryIndex = (xx * TABLE_ROW_SIZE) + zz;
    int byteIndex = entryIndex / 8;
    int bitIndex = entryIndex % 8;

    uint8_t tableByte = temple_angle_values[byteIndex];
    uint8_t mask = 0x01 << bitIndex;

    if ((tableByte & mask) != 0) {
        return true;
    }

    return false;
}