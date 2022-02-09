#include "Power.hpp"

#include <Arduino.h>
#include <algorithm>

#include "Misc/Platform.hpp"
#include "Misc/StringUtil.hpp"
#include "sindarin-debug.h"

void Power::setup() { this->lastMeasurementTime_ = millis(); }

void Power::loop() {
    auto now = millis();
    if (now > this->lastMeasurementTime_ + BATTERY_MEASUREMENT_PERIOD_SECONDS * 1000) {
        auto left = GetBatteryVoltageLeft();
        auto right = GetBatteryVoltageRight();
        this->leftConnected_ = left > BATTERY_DISCONNECT_THRESHOLD;
        this->rightConnected_ = right > BATTERY_DISCONNECT_THRESHOLD;

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
        if (this->getLeftBatteryVoltage() < BATTERY_MIN_VOLTAGE &&
            this->getRightBatteryVoltage() < BATTERY_MIN_VOLTAGE) {
            log_i("Voltage is below threshold. Halting.");
            this->depleted_ = true;
        }
        this->lastMeasurementTime_ = now;
    }
}

auto Power::average(const std::array<float, BATTERY_MEASUREMENTS_COUNT> &array, int count)
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

auto Power::getLeftBatteryVoltage() -> float {
    auto avg = this->average(this->leftMeasurements_, this->leftMeasurementCount_);
    log_d("Left: %.2fV %d %d", avg, this->leftIndex_, this->leftMeasurementCount_);
    return avg;
}

auto Power::getRightBatteryVoltage() -> float {
    auto avg = this->average(this->rightMeasurements_, this->rightMeasurementCount_);
    log_d("Right: %.2fV %d %d", avg, this->rightIndex_, this->rightMeasurementCount_);
    return avg;
}

auto Power::getBatteryLifeString() -> std::string {
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

auto Power::getBatteryPercentageOnlyString() -> std::string {
    float voltageLevelLeft = this->getLeftBatteryVoltage();
    float batteryFractionLeft =
        (voltageLevelLeft - BATTERY_MIN_VOLTAGE) / (BATTERY_MAX_VOLTAGE - BATTERY_MIN_VOLTAGE);
    float voltageLevelRight = this->getRightBatteryVoltage();
    float batteryFractionRight =
        (voltageLevelRight - BATTERY_MIN_VOLTAGE) / (BATTERY_MAX_VOLTAGE - BATTERY_MIN_VOLTAGE);
    batteryFractionLeft = std::clamp(batteryFractionLeft, 0.0f, 1.0f);
    batteryFractionRight = std::clamp(batteryFractionRight, 0.0f, 1.0f);
    float batteryAverage = (batteryFractionLeft + batteryFractionRight) / 2.0 * 100;
    std::string averageOutput = StringFormat("%.0f%%", batteryAverage);
    return averageOutput;
}

auto Power::getDepleted() -> bool {
#if SIMULATOR
    return false;
#else
    return this->depleted_;
#endif
}
