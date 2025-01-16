#pragma once

#include <array>
#include <vector>

#include "ADS1015.hpp"
#include "Pca9685.hpp"

class IRTrackerController {
public:
    // Structure to hold readings for each phototransistor when illuminated by each LED
    struct IRReadings {
        std::array<std::array<int16_t, 8>, 8> values{}; // [LED][sensor]
        std::array<std::array<float, 8>, 8> voltages{}; // [LED][sensor]
    };

    IRTrackerController(int i2cNum) : adc1_(i2cNum), adc2_(i2cNum), ledDriver_(i2cNum) {}

    // Initialize both ADCs and the LED driver
    auto init() -> bool;

    // Perform one complete scan cycle (each LED on in sequence)
    auto performScan() -> bool;

    // Get the last complete set of readings
    [[nodiscard]] auto getLastReadings() const -> const IRReadings &;

    // Check if specific devices are responding
    auto checkAdc1Available() -> bool;
    auto checkAdc2Available() -> bool;
    auto checkLedDriverAvailable() -> bool;

private:
    static constexpr uint8_t LEFT_LED_START = 0;  // First LED channel for left side
    static constexpr uint8_t RIGHT_LED_START = 4; // First LED channel for right side
    static constexpr uint8_t LEDS_PER_SIDE = 4;
    static constexpr uint16_t LED_ON = 4095; // Full brightness
    static constexpr uint16_t LED_OFF = 0;
    static constexpr int SETTLE_TIME_MS = 1; // Time to wait after LED state change

    Ads1015 adc1_;
    Ads1015 adc2_;
    Pca9685 ledDriver_;
    IRReadings lastReadings_{};
    bool configured_ = false;

    // Turn on a specific LED and turn all others off
    auto setActiveLed(uint8_t ledChannel) -> bool;

    // Read all 8 sensors with current LED configuration
    auto readAllSensors(uint8_t activeLed) -> bool;
};
