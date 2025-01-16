#include "IRTrackerController.hpp"

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "sindarin-debug.h"

auto IRTrackerController::init() -> bool {
    if (configured_) {
        return true;
    }

    // Initialize ADCs
    if (!adc1_.init()) {
        log_e("Failed to initialize first ADS1015");
        return false;
    }

    if (!adc2_.init()) {
        log_e("Failed to initialize second ADS1015");
        return false;
    }

    // Initialize LED driver
    if (!ledDriver_.init(1000)) { // 1kHz PWM frequency
        log_e("Failed to initialize PCA9685 LED driver");
        return false;
    }

    // Set gain for both ADCs
    adc1_.setGain(Ads1015Gain::GAIN_2_048V);
    adc2_.setGain(Ads1015Gain::GAIN_2_048V);

    // Turn all LEDs off initially
    ledDriver_.setAllPwm(LED_OFF);

    configured_ = true;
    return true;
}

auto IRTrackerController::performScan() -> bool {
    if (!configured_) {
        if (!init()) {
            return false;
        }
    }

    // Scan left side LEDs
    for (uint8_t i = 0; i < LEDS_PER_SIDE; i++) {
        uint8_t ledChannel = LEFT_LED_START + i;
        if (!setActiveLed(ledChannel)) {
            return false;
        }
        vTaskDelay(pdMS_TO_TICKS(SETTLE_TIME_MS));
        if (!readAllSensors(ledChannel)) {
            return false;
        }
    }

    // Scan right side LEDs
    for (uint8_t i = 0; i < LEDS_PER_SIDE; i++) {
        uint8_t ledChannel = RIGHT_LED_START + i;
        if (!setActiveLed(ledChannel)) {
            return false;
        }
        vTaskDelay(pdMS_TO_TICKS(SETTLE_TIME_MS));
        if (!readAllSensors(ledChannel)) {
            return false;
        }
    }

    // Turn all LEDs off after scan
    ledDriver_.setAllPwm(LED_OFF);
    return true;
}

auto IRTrackerController::setActiveLed(uint8_t ledChannel) -> bool {
    // Turn off all LEDs first
    if (!ledDriver_.setAllPwm(LED_OFF)) {
        return false;
    }

    // Turn on the selected LED
    return ledDriver_.setPwm(ledChannel, LED_ON);
}

auto IRTrackerController::readAllSensors(uint8_t activeLed) -> bool {
    // Read all 4 channels from first ADC
    for (int i = 0; i < 4; i++) {
        int16_t raw = adc1_.readSingleEnded(i);
        lastReadings_.values[activeLed][i] = raw;
        lastReadings_.voltages[activeLed][i] = adc1_.computeVolts(raw);
    }

    // Read all 4 channels from second ADC
    for (int i = 0; i < 4; i++) {
        int16_t raw = adc2_.readSingleEnded(i);
        lastReadings_.values[activeLed][i + 4] = raw;
        lastReadings_.voltages[activeLed][i + 4] = adc2_.computeVolts(raw);
    }

    return true;
}

auto IRTrackerController::getLastReadings() const -> const IRReadings & { return lastReadings_; }

auto IRTrackerController::checkAdc1Available() -> bool {
    if (!configured_) {
        if (!init()) {
            return false;
        }
    }
    return adc1_.readSingleEnded(0) != 0;
}

auto IRTrackerController::checkAdc2Available() -> bool {
    if (!configured_) {
        if (!init()) {
            return false;
        }
    }
    return adc2_.readSingleEnded(0) != 0;
}

auto IRTrackerController::checkLedDriverAvailable() -> bool {
    if (!configured_) {
        if (!init()) {
            return false;
        }
    }
    // Try to write to the LED driver
    return ledDriver_.setAllPwm(LED_OFF);
}
