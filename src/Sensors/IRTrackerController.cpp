#include "IRTrackerController.hpp"

#include "sindarin-debug.h"

auto IRTrackerController::init() -> bool {
    if (configured_) {
        return true;
    }

    // Initialize both ADCs
    if (!adc1_.init()) {
        log_e("Failed to initialize first ADS1015");
        return false;
    }

    if (!adc2_.init()) {
        log_e("Failed to initialize second ADS1015");
        return false;
    }

    // Set gain for both ADCs
    // Using 2/3x gain for maximum range of +/- 6.144V
    adc1_.setGain(Ads1015Gain::GAIN_6_144V);
    adc2_.setGain(Ads1015Gain::GAIN_6_144V);

    configured_ = true;
    return true;
}

auto IRTrackerController::readAllSensors() -> bool {
    if (!configured_) {
        if (!init()) {
            return false;
        }
    }

    // Read all 4 channels from first ADC
    for (int i = 0; i < 4; i++) {
        int16_t raw = adc1_.readSingleEnded(i);
        lastRawValues_[i] = raw;
        lastVoltages_[i] = adc1_.computeVolts(raw);
    }

    // Read all 4 channels from second ADC
    for (int i = 0; i < 4; i++) {
        int16_t raw = adc2_.readSingleEnded(i);
        lastRawValues_[i + 4] = raw;
        lastVoltages_[i + 4] = adc2_.computeVolts(raw);
    }

    return true;
}

auto IRTrackerController::getLastRawValue(int sensor) const -> int16_t {
    if (sensor >= 0 && sensor < NUM_SENSORS) {
        return lastRawValues_[sensor];
    }
    return 0;
}

auto IRTrackerController::getLastVoltage(int sensor) const -> float {
    if (sensor >= 0 && sensor < NUM_SENSORS) {
        return lastVoltages_[sensor];
    }
    return 0.0f;
}

auto IRTrackerController::checkAdc1Available() -> bool {
    if (!configured_) {
        if (!init()) {
            return false;
        }
    }
    // Try reading the first channel as a test
    int16_t value = adc1_.readSingleEnded(0);
    return value != 0;
}

auto IRTrackerController::checkAdc2Available() -> bool {
    if (!configured_) {
        if (!init()) {
            return false;
        }
    }
    // Try reading the first channel as a test
    int16_t value = adc2_.readSingleEnded(0);
    return value != 0;
}
