#include "BootError.hpp"

#include <Arduino.h>
#include <vector>

void BlinkBootErrors(const std::vector<BootError> &bootErrors) {
    // Blink out error codes:
    // TODO: How to cross-platfomr this?
    if (bootErrors.size() == 0) {
        digitalWrite(GREEN_LED, true);
        delay(100);
        digitalWrite(GREEN_LED, false);
        log_i("Error-free boot!");
    } else {
        for (BootError error : bootErrors) {
            log_e("Blinking error code %d for %s", error.type, error.string().c_str());
            for (int i = 0; i < error.blinks(); i++) {
                digitalWrite(RED_LED, true);
                delay(200);
                digitalWrite(RED_LED, false);
                delay(200);
            }
            delay(1000);
        }
    }
}