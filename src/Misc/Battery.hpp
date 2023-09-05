#pragma once

#define BATTERY_MEASUREMENT_PERIOD_SECONDS 5
#define BATTERY_LOGGING_PERIOD_SECONDS 60
#define BATTERY_MEASUREMENTS_COUNT 10
#define BATTERY_DISCONNECT_THRESHOLD 2.5F
// Battery hard cutoff is at 3V so let's shut off a little before that
#define BATTERY_MIN_VOLTAGE 3.4F
// Max LiPo cell voltage is 4.2 but as soon as its supplying current it tends to drop closer to 4V
#define BATTERY_MAX_VOLTAGE 4.15F

#include <array>
#include <string>

#include "Misc/Telemetry.hpp"
#include "Storage/FileSystem.hpp"
#include "Storage/PreferencesStore.hpp"

class Battery {
public:
    Battery(FileSystem &fs, PreferencesStore &prefs, Telemetry &telemetry)
        : fileSystem_(fs), prefs_(prefs), telemetry_(telemetry) {}
    Battery(const Battery &) = delete;

    void loop();

    auto getBatteryLifeString() -> std::string;
    auto getBatteryPercentageOnlyString() -> std::string;
    auto getLeftBatteryVoltage() -> float;
    auto getRightBatteryVoltage() -> float;
    auto getBatteryPercentage() -> float;

    auto getDepleted() -> bool;

    void saveLastBatteryReport();

protected:
    auto average(const std::array<float, BATTERY_MEASUREMENTS_COUNT> &array, int count) -> float;

    FileSystem &fileSystem_;
    PreferencesStore &prefs_;
    Telemetry &telemetry_;
    unsigned long lastMeasurementTime_ = 0;
    unsigned long lastLoggingTime_ = 0;
    bool depleted_;
    int leftIndex_;
    int rightIndex_;
    int leftMeasurementCount_;
    int rightMeasurementCount_;
    bool leftConnected_;
    bool rightConnected_;
    std::array<float, BATTERY_MEASUREMENTS_COUNT> leftMeasurements_;
    std::array<float, BATTERY_MEASUREMENTS_COUNT> rightMeasurements_;
};
