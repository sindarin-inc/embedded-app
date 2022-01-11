#pragma once

#define BATTERY_MEASUREMENT_PERIOD_SECONDS 5
#define BATTERY_MEASUREMENTS_COUNT 10
#define BATTERY_DISCONNECT_THRESHOLD 2.5F
// Battery hard cutoff is at 3V so let's shut off a little before that
#define BATTERY_MIN_VOLTAGE 3.1F
// Max LiPo cell voltage is 4.2 but as soon as its supplying current it tends to drop closer to 4V
#define BATTERY_MAX_VOLTAGE 4.0F

#include <Arduino.h>
#include <array>

class Power {
public:
    Power() = default;

    void setup();
    void loop();

    auto getBatteryLifeString() -> String;
    auto getLeftBatteryVoltage() -> float;
    auto getRightBatteryVoltage() -> float;

    auto getDepleted() -> bool;

protected:
    auto average(const std::array<float, BATTERY_MEASUREMENTS_COUNT> &array, int count) -> float;

    unsigned long lastMeasurementTime;
    bool depleted;
    int leftIndex;
    int rightIndex;
    int leftMeasurementCount;
    int rightMeasurementCount;
    bool leftConnected;
    bool rightConnected;
    std::array<float, BATTERY_MEASUREMENTS_COUNT> leftMeasurements;
    std::array<float, BATTERY_MEASUREMENTS_COUNT> rightMeasurements;
};
