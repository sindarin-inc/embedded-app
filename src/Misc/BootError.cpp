#include "BootError.hpp"

#include <driver/gpio.h>
#include <vector>

#include "Misc/Platform.hpp"
#include "config.h"
#include "sindarin-debug.h"

void BlinkBootErrors(const std::vector<BootError> &bootErrors) {
    // Un-hold the GPIOs (may have been held because the USB was connected)
    gpio_hold_dis((gpio_num_t)GREEN_LED);
    gpio_hold_dis((gpio_num_t)RED_LED);
    DigitalWrite(RED_LED, LED_OFF);
    DigitalWrite(GREEN_LED, LED_OFF);

    // Blink out error codes:
    // TODO: How to cross-platfomr this?
    if (bootErrors.size() == 0) {
        DigitalWrite(GREEN_LED, LED_ON);
        Delay(100);
        DigitalWrite(GREEN_LED, LED_OFF);
        log_i("Error-free boot!");
    } else {
        for (BootError error : bootErrors) {
            Delay(1000);
            log_e("Blinking error code %d for %s", error.type, error.string().c_str());
            for (int i = 0; i < error.blinks(); i++) {
                DigitalWrite(RED_LED, LED_ON);
                Delay(200);
                DigitalWrite(RED_LED, LED_OFF);
                Delay(200);
            }
            Delay(1000);
        }
    }
}