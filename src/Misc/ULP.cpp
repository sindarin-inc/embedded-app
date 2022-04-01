#if ULP_BACKLIGHT

#include <cstring>
#include <driver/rtc_io.h>
#include <esp32/ulp.h>
#include <hulp.h>

#include "sindarin-debug.h"

static RTC_DATA_ATTR ulp_var_t LcdBrightnessDelayOn;
static RTC_DATA_ATTR ulp_var_t LcdBrightnessDelayOff;

void UlpStart(void) {
    // Slow memory initialization
    memset(RTC_SLOW_MEM, 0, 512);
    // RTCGPIO7 / GPIO26 is the backlight
    // TODO: Hardcoded for now
    const gpio_num_t ledPWMPin = GPIO_NUM_26;
    // Datasheet says offset the RTC_GPIO # by 14 and that's the correct bit to set in
    // RTC_GPIO_OUT_REG
    const int ledPWMBit = RTCIO_GPIO26_CHANNEL + 14;

    enum {
        LBL_LOOP = 0,
        LBL_WAIT_HIGH,
        LBL_WAIT_LOW,
    };

    // Apparently using the macro-style ULP programming is deprecated and we're supposed to use the
    // more standard .S assembly file. See:
    // https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/system/ulp.html
    // The HULP helper libraries were handy, and those only work with macros, and it's kinda nice
    // that we could make our own macros, but probably worth moving to the more standardized format
    // at some point. See also the macro reference:
    // https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/system/ulp_macros.html
    const ulp_insn_t ulp_prog[] = {
        M_LABEL(LBL_LOOP),

        // Write 1 to the GPIO_OUT register's appropriate bit (turn on)
        I_WR_REG_BIT(RTC_GPIO_OUT_REG, ledPWMBit, 0),

        // Load the on delay value from memory into R3
        I_MOVI(R3, 0),
        I_GET(R0, R3, LcdBrightnessDelayOff),

        // Delay for R0 # of loops
        M_LABEL(LBL_WAIT_HIGH),
        I_SUBI(R0, R0, 1),
        I_DELAY(10),
        // If we're >= 1, then keep looping
        M_BGE(LBL_WAIT_HIGH, 1),

        // Write 0 to the GPIO_OUT register's appropriate bit (turn off)
        // TODO: I_WR_REG_BIT is probably more right here
        I_WR_REG_BIT(RTC_GPIO_OUT_REG, ledPWMBit, 1),

        // Load the off delay value from memory into R3
        I_MOVI(R3, 0),
        I_GET(R0, R3, LcdBrightnessDelayOn),

        // Delay for R0 # of loops
        M_LABEL(LBL_WAIT_LOW),
        I_SUBI(R0, R0, 1),
        I_DELAY(10),
        // If we're >= 1, then keep looping
        M_BGE(LBL_WAIT_LOW, 1),

        // Branch back to the top of the loop
        M_BX(LBL_LOOP),
    };

    ESP_ERROR_CHECK(hulp_configure_pin(ledPWMPin, RTC_GPIO_MODE_OUTPUT_ONLY, GPIO_FLOATING, 0));
    ESP_ERROR_CHECK(hulp_ulp_load(ulp_prog, sizeof(ulp_prog), 1000UL * 5, 0));
    ESP_ERROR_CHECK(hulp_ulp_run(0));
    log_d("ULP started");
}

void UlpSetBrightness(uint8_t brightness) {
    LcdBrightnessDelayOn.val = brightness;
    LcdBrightnessDelayOff.val = 255 - brightness;
    log_d("Set ULP brightness to %d", LcdBrightnessDelayOn.val);
}

#endif
