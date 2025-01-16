#pragma once

#include <array>

#include "ADS1015.hpp"

class IRTrackerController {
public:
    // Constructor takes two I2C addresses for the two ADS1015 chips
    IRTrackerController(int i2cNum) : adc1_(i2cNum), adc2_(i2cNum) {}

    // Initialize both ADC chips
    auto init() -> bool;

    // Read all 8 IR sensor values at once
    auto readAllSensors() -> bool;

    // Get the last read values
    [[nodiscard]] auto getLastRawValue(int sensor) const -> int16_t;
    [[nodiscard]] auto getLastVoltage(int sensor) const -> float;

    // Check if a specific ADC is responding
    auto checkAdc1Available() -> bool;
    auto checkAdc2Available() -> bool;

private:
    static constexpr uint8_t ADC1_ADDR = 0x48; // 0b1001000
    static constexpr uint8_t ADC2_ADDR = 0x49; // 0b1001001
    static constexpr int NUM_SENSORS = 8;

    Ads1015 adc1_;
    Ads1015 adc2_;

    // Store the last read values from all sensors
    std::array<int16_t, NUM_SENSORS> lastRawValues_{};
    std::array<float, NUM_SENSORS> lastVoltages_{};

    bool configured_ = false;
};
