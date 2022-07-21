#if !SIMULATOR

#include <driver/i2c.h>
#include <driver/ledc.h>
#include <driver/uart.h>
#include <esp_adc_cal.h>
#include <soc/rtc.h>

#include "Platform.hpp"
#include "RTOS.h"
#include "config.h"
#include "sindarin-debug.h"

#if ULP_BACKLIGHT
#include "Misc/ULP.hpp"
#endif

#define UPPER_DIVIDER 442
#define LOWER_DIVIDER 160
#define DEFAULT_VREF 1100 // Default referance voltage in mv
#define BACKLIGHT_PWM_CHANNEL 0

void InitializePlatform() {
    pinMode(BAT_VOLTAGE_LEFT, ANALOG);
    analogSetPinAttenuation(BAT_VOLTAGE_LEFT, ADC_6db);

    pinMode(BAT_VOLTAGE_RIGHT, ANALOG);
    analogSetPinAttenuation(BAT_VOLTAGE_RIGHT, ADC_6db);

    pinMode(RED_LED, OUTPUT);
    pinMode(GREEN_LED, OUTPUT);

#if USE_I2C
    // Initialize I2C port 0
    int i2cPort = 0;
    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = I2C_MASTER_SDA_IO,
        .scl_io_num = I2C_MASTER_SCL_IO,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
    };
    conf.master.clk_speed = I2C_MASTER_FREQ_HZ;
    i2c_param_config(i2cPort, &conf);
    i2c_driver_install(i2cPort, conf.mode, 0, 0, 0);
#endif

#if EINKET013TT1
#ifdef DISPLAY_POWER
    pinMode(DISPLAY_POWER, OUTPUT);
#endif
#if LEDC_BACKLIGHT
    // Backlight configs
    ledc_timer_config_t ledcTimer = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .duty_resolution = LEDC_TIMER_8_BIT,
        .timer_num = LEDC_TIMER_0,
        .freq_hz = 1000,
        .clk_cfg = LEDC_USE_RTC8M_CLK //
    };
    ledc_timer_config(&ledcTimer);

    ledc_channel_config_t ledcChannel = {
        .gpio_num = BACKLIGHT_LEFT,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel = LEDC_CHANNEL_0,
        .intr_type = LEDC_INTR_DISABLE,
        .timer_sel = LEDC_TIMER_0,
        .duty = 0,
        .hpoint = 0 //
    };

    ledc_channel_config(&ledcChannel);
    ledcChannel.gpio_num = BACKLIGHT_RIGHT;
    ledc_channel_config(&ledcChannel);
#endif
#if ULP_BACKLIGHT
    UlpStart();
#endif
#endif // EINKET013TT1
}

// Forked from TinyPico's GetBatteryVoltage() method:
// https://github.com/tinypico/tinypico-arduino/blob/master/TinyPICO-Helper/src/TinyPICO.cpp#L207
auto GetBatteryVoltageLeft() -> float {
    uint32_t raw, mv;
    esp_adc_cal_characteristics_t chars;

    // only check voltage every 1 second

    // grab latest voltage
    analogRead(BAT_VOLTAGE_LEFT);          // Just to get the ADC setup
    raw = adc1_get_raw(BATT_CHANNEL_LEFT); // Read of raw ADC value

    // Get ADC calibration values
    esp_adc_cal_characterize(ADC_UNIT_1, ADC_ATTEN_6db, ADC_WIDTH_BIT_12, DEFAULT_VREF, &chars);

    // Convert to calibrated mv then volts
    mv = esp_adc_cal_raw_to_voltage(raw, &chars) * (LOWER_DIVIDER + UPPER_DIVIDER) / LOWER_DIVIDER;
    return (float)mv / 1000.0;
}

auto GetBatteryVoltageRight() -> float {
    uint32_t raw, mv;
    esp_adc_cal_characteristics_t chars;

    // grab latest voltage
    analogRead(BAT_VOLTAGE_RIGHT);          // Just to get the ADC setup
    raw = adc1_get_raw(BATT_CHANNEL_RIGHT); // Read of raw ADC value

    // Get ADC calibration values
    esp_adc_cal_characterize(ADC_UNIT_1, ADC_ATTEN_6db, ADC_WIDTH_BIT_12, DEFAULT_VREF, &chars);

    // Convert to calibrated mv then volts
    mv = esp_adc_cal_raw_to_voltage(raw, &chars) * (LOWER_DIVIDER + UPPER_DIVIDER) / LOWER_DIVIDER;

    return (float)mv / 1000.0;
}

void SetCpuSpeedMhz(int speedMhz) {
    // This is a no-op if we're using automatic power management
#if DISABLE_POWER_MANAGEMENT
    if (speedMhz == esp_clk_cpu_freq() / 1000000) {
        return;
    }
    if (setCpuFrequencyMhz(speedMhz) != true) {
        log_e("Failed to setCpuFrequencyMhz(%d)", speedMhz);
    }
#endif
}

void SetBacklight(int percent) {
    auto dutyCycle = percent * 255 / 100;
    log_d("Setting backlight to %d%% (%d)", percent, dutyCycle);
#if LEDC_BACKLIGHT
    ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, dutyCycle);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);
#endif
#if ULP_BACKLIGHT
    UlpSetBrightness(dutyCycle);
#endif
}

auto GetRunTimeMs() -> uint32_t { return FreeRTOS::getTimeSinceStart(); }

#endif
