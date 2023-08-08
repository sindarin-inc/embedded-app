#include "Battery.hpp"

#include <algorithm>

#include "Misc/Dispatch.hpp"
#include "Misc/Platform.hpp"
#include "Misc/StringUtil.hpp"
#include "sindarin-debug.h"

// Currently 1MB
#define MAX_VOLTAGE_LOG_SIZE 1000000

void Battery::loop() {
    auto now = Millis();

    if (this->lastMeasurementTime_ == 0 ||
        now > this->lastMeasurementTime_ + BATTERY_MEASUREMENT_PERIOD_SECONDS * 1000) {
        auto left = GetBatteryVoltageLeft();
        auto right = GetBatteryVoltageRight();
        this->leftConnected_ = left > BATTERY_DISCONNECT_THRESHOLD;
        this->rightConnected_ = right > BATTERY_DISCONNECT_THRESHOLD;

        if (now > lastLoggingTime_ + BATTERY_LOGGING_PERIOD_SECONDS * 1000) {
            DispatchGetCore1BackgroundQueue()->dispatch([this, left, right]() {
                telemetry_.logMetricData(TelemetryField::METRIC_BATTERY_VOLTAGE,
                                         {{"l", left}, {"r", right}});
            });
            lastLoggingTime_ = now;
        }

        // Save into the array as a circular buffer of up to BATTERY_MEASUREMENTS_COUNT most recent
        // values, but only if the battery is connected. We don't want to mess up the averages if
        // the battery is disconnected temporarily.
        if (this->leftConnected_) {
            this->leftMeasurements_[this->leftIndex_] = left;
            this->leftIndex_ = (this->leftIndex_ + 1) % BATTERY_MEASUREMENTS_COUNT;
            this->leftMeasurementCount_ =
                std::min(this->leftMeasurementCount_ + 1, BATTERY_MEASUREMENTS_COUNT);
        } else {
            this->leftMeasurementCount_ = 0;
            this->leftIndex_ = 0;
        }
        if (this->rightConnected_) {
            this->rightMeasurements_[this->rightIndex_] = right;
            this->rightIndex_ = (this->rightIndex_ + 1) % BATTERY_MEASUREMENTS_COUNT;
            this->rightMeasurementCount_ =
                std::min(this->rightMeasurementCount_ + 1, BATTERY_MEASUREMENTS_COUNT);
        } else {
            this->rightMeasurementCount_ = 0;
            this->rightIndex_ = 0;
        }

        // Check if both batteries are below the threshold. If so, show the battery screen and halt
        // everything
#if !DISABLE_LOW_VOLTAGE_HALT
        auto leftBatteryVoltage = this->getLeftBatteryVoltage();
        auto rightBatteryVoltage = this->getRightBatteryVoltage();
        if (leftBatteryVoltage < BATTERY_MIN_VOLTAGE && rightBatteryVoltage < BATTERY_MIN_VOLTAGE &&
            (this->leftConnected_ || this->rightConnected_)) {
            log_i("Voltage is below threshold. Halting. (L: %.2fV, R: %.2fV, < %.2fV)",
                  leftBatteryVoltage, rightBatteryVoltage, BATTERY_MIN_VOLTAGE);
            this->depleted_ = true;
        }
#endif
        this->lastMeasurementTime_ = now;
    }
}

auto Battery::average(const std::array<float, BATTERY_MEASUREMENTS_COUNT> &array, int count)
    -> float {
    if (count == 0) {
        return 0;
    }
    float sum = 0;
    for (int i = 0; i < count; i++) {
        sum += array[i];
    }
    return sum / count;
}

auto Battery::getLeftBatteryVoltage() -> float {
    auto avg = this->average(this->leftMeasurements_, this->leftMeasurementCount_);
    return avg;
}

auto Battery::getRightBatteryVoltage() -> float {
    auto avg = this->average(this->rightMeasurements_, this->rightMeasurementCount_);
    return avg;
}

auto Battery::getBatteryLifeString() -> std::string {
    float voltageLevelLeft = this->getLeftBatteryVoltage();
    float batteryFractionLeft =
        (voltageLevelLeft - BATTERY_MIN_VOLTAGE) / (BATTERY_MAX_VOLTAGE - BATTERY_MIN_VOLTAGE);
    float voltageLevelRight = this->getRightBatteryVoltage();
    float batteryFractionRight =
        (voltageLevelRight - BATTERY_MIN_VOLTAGE) / (BATTERY_MAX_VOLTAGE - BATTERY_MIN_VOLTAGE);
    batteryFractionLeft = std::clamp(batteryFractionLeft, 0.0f, 1.0f);
    batteryFractionRight = std::clamp(batteryFractionRight, 0.0f, 1.0f);
    std::string batteryOutput =
        StringFormat("L: %.2fV (%.0f%%) R: %.2fV (%.0f%%)", voltageLevelLeft,
                     batteryFractionLeft * 100, voltageLevelRight, batteryFractionRight * 100);
    log_d("Battery: %s", batteryOutput.c_str());
    return batteryOutput;
}

auto Battery::getBatteryPercentageOnlyString() -> std::string {
    float batteryAverage = getBatteryPercentage();
    std::string averageOutput = StringFormat("%.0f%%", batteryAverage);
    return averageOutput;
}

auto Battery::getBatteryPercentage() -> float {
    float voltageLevelLeft = this->getLeftBatteryVoltage();
    float batteryFractionLeft =
        (voltageLevelLeft - BATTERY_MIN_VOLTAGE) / (BATTERY_MAX_VOLTAGE - BATTERY_MIN_VOLTAGE);
    float voltageLevelRight = this->getRightBatteryVoltage();
    float batteryFractionRight =
        (voltageLevelRight - BATTERY_MIN_VOLTAGE) / (BATTERY_MAX_VOLTAGE - BATTERY_MIN_VOLTAGE);
    batteryFractionLeft = std::clamp(batteryFractionLeft, 0.0f, 1.0f);
    batteryFractionRight = std::clamp(batteryFractionRight, 0.0f, 1.0f);
    float batteryAverage = (batteryFractionLeft + batteryFractionRight) / 2.0 * 100;

    // log_d("Battery: %.0f%% (left: %fV, right %fV)", batteryAverage, voltageLevelLeft,
    // voltageLevelRight);

    if ((int)batteryAverage == 0) {
        batteryAverage =
            PreferencesStore::lastBatteryReport == 0 ? 90 : PreferencesStore::lastBatteryReport;
    }

    return batteryAverage;
}

auto Battery::getDepleted() -> bool {
#if SIMULATOR
    return false;
#else
    return this->depleted_;
#endif
}

void Battery::saveLastBatteryReport() {
    float batteryAverage = getBatteryPercentage();
    if (batteryAverage > 0) {
        PreferencesStore::lastBatteryReport = uint8_t(batteryAverage);
    }
}
