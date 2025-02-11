#include "BootError.hpp"

#include <driver/gpio.h>
#include <vector>

#include "Misc/Dispatch.hpp"
#include "Misc/Platform.hpp"
#include "config.h"
#include "sindarin-debug.h"

void BlinkBootErrors(const std::vector<BootError> &bootErrors) {
    // Un-hold the GPIOs (may have been held because the USB was connected)
#if CONFIG_GREEN_LED != -1
    gpio_hold_dis(static_cast<gpio_num_t>(CONFIG_GREEN_LED));
    DigitalWrite(CONFIG_RED_LED, LED_OFF);
#endif
#if CONFIG_RED_LED != -1
    gpio_hold_dis(static_cast<gpio_num_t>(CONFIG_RED_LED));
    DigitalWrite(CONFIG_GREEN_LED, LED_OFF);
#endif

    // Blink out error codes:
    // TODO: How to cross-platfomr this?
    if (bootErrors.empty()) {
        log_i("Error-free boot!");
        DispatchGetCore0BackgroundQueue()->dispatch([]() {
#if CONFIG_GREEN_LED != -1
            DigitalWrite(CONFIG_GREEN_LED, LED_ON);
            Delay(100);
            DigitalWrite(CONFIG_GREEN_LED, LED_OFF);
#endif
        });
    } else {
        for (BootError error : bootErrors) {
            log_e("Blinking error code %d for %s", error.type, error.string().c_str());
            Delay(1000);
            for (int i = 0; i < error.blinks(); i++) {
#if CONFIG_RED_LED != -1
                DigitalWrite(CONFIG_RED_LED, LED_ON);
                Delay(200);
                DigitalWrite(CONFIG_RED_LED, LED_OFF);
                Delay(200);
#endif
            }
            Delay(1000);
        }
    }
}