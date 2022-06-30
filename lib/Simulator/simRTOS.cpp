#include "simRTOS.h"
#include "Arduino.h"

uint32_t FreeRTOS::getTimeSinceStart() {
    return millis();
}