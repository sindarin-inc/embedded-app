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

void vPortYield();

void digitalWrite(int, int);

#define HIGH 1
#define LOW 0

void initArduino();
