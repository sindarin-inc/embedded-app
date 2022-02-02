#include "SPI.h"

#include <stdio.h>

SPIClass SPI;

void SPIClass::beginTransaction(SPISettings settings) { printf("SPI["); }

void SPIClass::endTransaction(void) { printf("]\n"); }

uint8_t SPIClass::transfer(uint8_t data) {
    printf("%02X,", data);
    return 0;
}

void SPIClass::write(uint8_t data) { printf("%02X,", data); }

void SPIClass::writeBytes(const uint8_t *data, uint32_t size) {
    for (int i = 0; i < size; i++) {
        printf("%02X,", data[i]);
    }
}

void SPIClass::begin(int8_t sck, int8_t miso, int8_t mosi, int8_t ss) {}

SPIClass::SPIClass() {}