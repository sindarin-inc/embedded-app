#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include <inttypes.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "Print.h"
#include "Printable.h"
#include "Stream.h"
#include "WString.h"
#include "esp32-hal-log.h"
#include "esp32-hal.h"

typedef bool boolean;

void delay(int delay_ms);

void esp_deep_sleep_start();

void esp_restart();

void pinMode(uint8_t pin, uint8_t mode);
void digitalWrite(uint8_t pin, uint8_t val);
int digitalRead(uint8_t pin);

#define LOW 0x0
#define HIGH 0x1

// GPIO FUNCTIONS
#define INPUT 0x01
#define OUTPUT 0x02
#define PULLUP 0x04
#define INPUT_PULLUP 0x05
#define PULLDOWN 0x08
#define INPUT_PULLDOWN 0x09
#define OPEN_DRAIN 0x10
#define OUTPUT_OPEN_DRAIN 0x12
#define SPECIAL 0xF0
#define FUNCTION_1 0x00
#define FUNCTION_2 0x20
#define FUNCTION_3 0x40
#define FUNCTION_4 0x60
#define FUNCTION_5 0x80
#define FUNCTION_6 0xA0
#define ANALOG 0xC0

#define CCOUNT 234
