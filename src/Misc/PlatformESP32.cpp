#if !SIMULATOR

#include <driver/uart.h>
#include <esp_adc_cal.h>

#include "Platform.hpp"
#include "config.h"
#include "sindarin-debug.h"

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

#if REVISION < 7
    pinMode(DISPLAY_POWER, OUTPUT);
#else
    // Backlight
    const int pwmFreq = 5000;
    const int pwmResolution = 8;
    ledcSetup(BACKLIGHT_PWM_CHANNEL, pwmFreq, pwmResolution);

    // Attach the LED PWM Channel to the GPIO Pins
    ledcAttachPin(BACKLIGHT_LEFT, BACKLIGHT_PWM_CHANNEL);
    ledcAttachPin(BACKLIGHT_RIGHT, BACKLIGHT_PWM_CHANNEL);
#endif
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
    if (speedMhz == esp_clk_cpu_freq() / 1000000) {
        return;
    }
    // Wait for the Serial buffer to flush otherwise we may get corruption
    esp_err_t err = uart_wait_tx_done(0, 100);
    if (err != ESP_OK) {
        log_e("uart_wait_tx_done failed with code %d", err);
    }
    if (setCpuFrequencyMhz(speedMhz) != true) {
        log_e("Failed to setCpuFrequencyMhz(%d)", speedMhz);
    }
}

void SetBacklight(int percent) {
    auto dutyCycle = percent * 255 / 100;
    log_d("Setting backlight to %d%% (%d)", percent, dutyCycle);
#if REVISION >= 7
    ledcWrite(BACKLIGHT_PWM_CHANNEL, dutyCycle);
#endif
}

#endif
