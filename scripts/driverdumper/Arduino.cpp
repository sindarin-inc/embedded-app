#include <stdint.h>
#include <stdio.h>

void pinMode(uint8_t pin, uint8_t mode) {}

void digitalWrite(uint8_t pin, uint8_t val) { printf("digitalWrite(%d,%d)\n", pin, val); }

int digitalRead(uint8_t pin) { return 0; }

void delay(uint32_t ms) { printf("delay(%d)\n", ms); }