#include "Platform.hpp"

#include <esp_adc_cal.h>

#include "config.h"

#include "sindarin-debug.h"

#define UPPER_DIVIDER 442
#define LOWER_DIVIDER 160
#define DEFAULT_VREF 1100 // Default referance voltage in mv

void InitializePlatform() {
    pinMode(BAT_VOLTAGE_LEFT, ANALOG);
    analogSetPinAttenuation(BAT_VOLTAGE_LEFT, ADC_6db);

    pinMode(BAT_VOLTAGE_RIGHT, ANALOG);
    analogSetPinAttenuation(BAT_VOLTAGE_RIGHT, ADC_6db);

    pinMode(RED_LED, OUTPUT);
    pinMode(GREEN_LED, OUTPUT);

    pinMode(DISPLAY_POWER, OUTPUT);
}

// Forked from TinyPico's GetBatteryVoltage() method:
// https://github.com/tinypico/tinypico-arduino/blob/master/TinyPICO-Helper/src/TinyPICO.cpp#L207
float GetBatteryVoltageLeft() {
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

float GetBatteryVoltageRight() {
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
