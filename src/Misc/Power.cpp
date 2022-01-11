#include "Power.hpp"

#include <Arduino.h>

#include "Misc/Platform.hpp"
#include "sindarin-debug.h"

void Power::setup() { this->lastMeasurementTime = millis(); }

void Power::loop() {
    auto now = millis();
    if (now - BATTERY_MEASUREMENT_PERIOD_SECONDS * 1000 > this->lastMeasurementTime) {
        auto left = GetBatteryVoltageLeft();
        auto right = GetBatteryVoltageRight();
        this->leftConnected = left > BATTERY_DISCONNECT_THRESHOLD;
        this->rightConnected = right > BATTERY_DISCONNECT_THRESHOLD;

        // Save into the array as a circular buffer of up to BATTERY_MEASUREMENTS_COUNT most recent
        // values, but only if the battery is connected. We don't want to mess up the averages if
        // the battery is disconnected temporarily.
        if (this->leftConnected) {
            this->leftMeasurements[this->leftIndex] = left;
            this->leftIndex = (this->leftIndex + 1) % BATTERY_MEASUREMENTS_COUNT;
            this->leftMeasurementCount =
                std::min(this->leftMeasurementCount + 1, BATTERY_MEASUREMENTS_COUNT);
        } else {
            this->leftMeasurementCount = 0;
            this->leftIndex = 0;
        }
        if (this->rightConnected) {
            this->rightMeasurements[this->rightIndex] = right;
            this->rightIndex = (this->rightIndex + 1) % BATTERY_MEASUREMENTS_COUNT;
            this->rightMeasurementCount =
                std::min(this->rightMeasurementCount + 1, BATTERY_MEASUREMENTS_COUNT);
        } else {
            this->rightMeasurementCount = 0;
            this->rightIndex = 0;
        }

        // Check if both batteries are below the threshold. If so, show the battery screen and halt
        // everything
        if (this->getLeftBatteryVoltage() < BATTERY_MIN_VOLTAGE &&
            this->getRightBatteryVoltage() < BATTERY_MIN_VOLTAGE) {
            log_i("Voltage is below threshold. Halting.");
            this->depleted = true;
        }
        this->lastMeasurementTime = now;
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
    auto avg = this->average(this->leftMeasurements, this->leftMeasurementCount);
    log_d("Left: %.2fV %d %d", avg, this->leftIndex, this->leftMeasurementCount);
    return avg;
}

auto Power::getRightBatteryVoltage() -> float {
    auto avg = this->average(this->rightMeasurements, this->rightMeasurementCount);
    log_d("Right: %.2fV %d %d", avg, this->rightIndex, this->rightMeasurementCount);
    return avg;
}

// std::clamp is not working even though it's supposed to be available since C++17
#define clamp(v, lo, hi) (((v) < (lo)) ? (lo) : ((v) > (hi)) ? (hi) : (v))

auto Power::getBatteryLifeString() -> String {
    float voltageLevelLeft = this->getLeftBatteryVoltage();
    float batteryFractionLeft =
        (voltageLevelLeft - BATTERY_MIN_VOLTAGE) / (BATTERY_MAX_VOLTAGE - BATTERY_MIN_VOLTAGE);
    float voltageLevelRight = this->getRightBatteryVoltage();
    float batteryFractionRight =
        (voltageLevelRight - BATTERY_MIN_VOLTAGE) / (BATTERY_MAX_VOLTAGE - BATTERY_MIN_VOLTAGE);
    batteryFractionLeft = clamp(batteryFractionLeft, 0, 1.0);
    batteryFractionRight = clamp(batteryFractionRight, 0, 1.0);
    String battery_output = " Raw L:" + String(voltageLevelLeft, 2) +
                            " R:" + String(voltageLevelRight, 2) +
                            "  L:" + String(batteryFractionLeft * 100) +
                            "% R:" + String(batteryFractionRight * 100) + "%";

    log_d("Power: L: %.2f (%.0f%%) R: %.2f (%.0f%%)", voltageLevelLeft, batteryFractionLeft * 100,
          voltageLevelRight, batteryFractionRight * 100);
    return battery_output;
}

auto Power::getDepleted() -> bool { return this->depleted; }
