#include "BootError.hpp"

#include <driver/gpio.h>
#include <vector>

#include "Misc/Dispatch.hpp"
#include "Misc/Platform.hpp"
#include "config.h"
#include "sindarin-debug.h"

void BlinkBootErrors(const std::vector<BootError> &bootErrors) {
    // Un-hold the GPIOs (may have been held because the USB was connected)
    gpio_hold_dis(static_cast<gpio_num_t>(CONFIG_GREEN_LED));
    gpio_hold_dis(static_cast<gpio_num_t>(CONFIG_RED_LED));
    DigitalWrite(CONFIG_RED_LED, LED_OFF);
    DigitalWrite(CONFIG_GREEN_LED, LED_OFF);

    // Blink out error codes:
    // TODO: How to cross-platfomr this?
    if (bootErrors.empty()) {
        log_i("Error-free boot!");
        DispatchGetCore0BackgroundQueue()->dispatch([]() {
            DigitalWrite(CONFIG_GREEN_LED, LED_ON);
            Delay(100);
            DigitalWrite(CONFIG_GREEN_LED, LED_OFF);
        });
    } else {
        for (BootError error : bootErrors) {
            log_e("Blinking error code %d for %s", error.type, error.string().c_str());
            Delay(1000);
            for (int i = 0; i < error.blinks(); i++) {
                DigitalWrite(CONFIG_RED_LED, LED_ON);
                Delay(200);
                DigitalWrite(CONFIG_RED_LED, LED_OFF);
                Delay(200);
            }
            Delay(1000);
        }
    }
}