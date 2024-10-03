#include "LightingController.hpp"

#include <cmath>
#include <esp_system.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "Misc/Math.hpp"
#include "Misc/Platform.hpp"
#include "sindarin-debug.h"

auto LightingController::checkSensorAvailable() -> bool {
    // In case we're still operating from a software reset, attempt to disable the light sensor so
    // we can start fresh
    auto resetReason = esp_reset_reason();
    if (resetReason == ESP_RST_SW || resetReason == ESP_RST_INT_WDT ||
        resetReason == ESP_RST_TASK_WDT || resetReason == ESP_RST_WDT) {
        lightSensor_.disable();
    }

    auto partId = lightSensor_.readPartId();
    auto manufacturerId = lightSensor_.readManufacturerId();
    if (partId != 0xA0 || manufacturerId != 0x05) {
        log_e("LTR329 ambient light sensor not found (pargtId: 0x%02X, manufacturerId: 0x%02X)",
              partId, manufacturerId);
        return false;
    } else {
        log_i("LTR329 ambient light sensor found (partId: 0x%02X, manufacturerId: 0x%02X)", partId,
              manufacturerId);
        return true;
    }
}

void LightingController::begin() {
    if (enabled_) {
        return;
    }
    // Not pictured here: all the LEDC configuration that happens in PlatformESP32.cpp. We keep that
    // code over there because the LEDC driver is configured globally and we'll probably also want
    // to configure the RGB LEDs with LEDC eventually
    lightSensor_.enable();
    enabled_ = true;
}

void LightingController::shutdown(int fadeMs) {
    if (!enabled_) {
        return;
    }
    enabled_ = false;
    lightSensor_.disable();
    setCurrentDutyCycle(0, fadeMs);
}

void LightingController::setOverrideValuePercent(int percent, bool userInitiated) {
    auto dutyCycle = percent / 7;
    if (dutyCycle == 0) {
        dutyCycle = 1;
    } else if (dutyCycle == 1) {
        dutyCycle = 2;
    } else {
        dutyCycle *= dutyCycle;
    }

    if (userInitiated) {
        // Note the difference in the predicted value and the override value
        int diff = dutyCycle - readPredictedDutyCycle();
        // Limit diff to +/- 60
        diff = std::clamp(diff, -60, 60);
        prefs_.brightnessOffset.set(diff);
    }

    log_d("Override duty cycle: %d (%d%%)", dutyCycle, percent);
    overrideDutyCycle_ = dutyCycle;

    setCurrentDutyCycle(overrideDutyCycle_, 100);
}

auto LightingController::getCurrentDutyCycle() const -> int {
    if (overrideDutyCycle_ != -1) {
        return overrideDutyCycle_;
    } else {
        return predictedDutyCycle_;
    }
}

auto LightingController::getCurrentPercent() const -> int {
    auto dutyCycle = getCurrentDutyCycle();
    if (dutyCycle == 1) {
        return 0;
    } else if (dutyCycle == 2) {
        return 7;
    } else {
        return static_cast<int>(std::sqrt(dutyCycle)) * 7;
    }
}

void LightingController::setScreenMask(Screens screens, int fadeMs) {
    log_d("Setting screens mask to %x", screens);
    screens_ = screens;
    setCurrentDutyCycle(predictedDutyCycle_, fadeMs);
};

auto LightingController::readAmbientLightData(uint16_t &channel1, uint16_t &channel0) -> bool {
    if (!lightSensor_.readData(channel1, channel0)) {
        // Failed to read data, so just return false
        return false;
    }
    return true;
}

auto LightingController::readPredictedDutyCycle() -> int {
    auto average = movingAverage_.average();
    return MapRange(average, 0, 1000, static_cast<int>(GetMinLightingDutyCycle()),
                    static_cast<int>(GetMaxLightingDutyCycle()));
}

auto LightingController::scaleLux(float lux) -> float { return std::log10(lux + 1) * 300; }

auto LightingController::luxify(uint16_t channel0, uint16_t channel1) -> float {
    float ratio = static_cast<float>(channel1) / (channel0 + channel1);
    float lux = 0.0;

    if (ratio < 0.45) {
        lux = (1.7743 * channel0 + 1.1059 * channel1) / 1000;
    } else if (ratio < 0.64 && ratio >= 0.45) {
        lux = (4.2785 * channel0 - 1.9548 * channel1) / 1000;
    } else if (ratio < 0.85 && ratio >= 0.64) {
        lux = (0.5926 * channel0 + 0.1185 * channel1) / 1000;
    } else {
        lux = 0;
    }

    return lux;
}

void LightingController::checkAmbientLight(int fadeMs, int timeoutMs) {
    if (!enabled_) {
        log_i("Ambient light check requested but lights are out, ignoring");
        return;
    }

    uint16_t channel1, channel0;
    if (!readAmbientLightData(channel1, channel0)) {
        // Failed to read data, so just give up and return (will be logged from the Ltr329Als
        // driver)
        log_w("Failed to read ambient light data from LTR329ALS");
        return;
    }

    float scaledLux = scaleLux(luxify(channel0, channel1));

    // Clamp to a max value of 1000 to avoid the moving average getting too large
    movingAverage_.add(std::min(static_cast<uint16_t>(scaledLux), static_cast<uint16_t>(1000)));

    // Adjust based on the offset
    predictedDutyCycle_ =
        std::clamp(readPredictedDutyCycle() + prefs_.brightnessOffset.get().value_or(0), 1,
                   static_cast<int>(GetMaxLightingDutyCycle() * 80 / 100));

    // Here for testing as we dial in the values
    // log_d("Setting duty to predicted duty cycle: %d %d channel0: %d channel1: %d with a lux of
    // %f", predictedDutyCycle_, readPredictedDutyCycle(), channel0, channel1, scaledLux);

    // Bail out early if we have overridden the value here
    if (overrideDutyCycle_ != -1) {
        return;
    }

    setCurrentDutyCycle(predictedDutyCycle_, fadeMs);
}

void LightingController::setCurrentDutyCycle(int dutyCycle, int fadeMs) {
    int leftDutyCycle = (screens_ & LEFT) ? dutyCycle : 0;
    int rightDutyCycle = (screens_ & RIGHT) ? dutyCycle : 0;
    // log_d("Setting screen light to l=%d/255 r=%d/255 (fadeMs=%d) screens_=%x", leftDutyCycle,
    //       rightDutyCycle, fadeMs, screens_);
    if (fadeMs > 0) {
        // NOTE(john): This will crash if either of these are 0 and idf.py monitor is connected.
        // This is because the LEDC driver uses the 32kHz clock and the 32kHz clock doesn't work
        // with the serial monitor (for some unknown reason). The normal check that prevents crashes
        // for LEDC is checking for dutyCycle <= maxDutyCycle, and when the configuration fails,
        // maxDutyCycle is 0. So 0 uniquely gets through this check, and later causes a crash on a
        // divide by zero.
        // See
        // https://github.com/espressif/esp-idf/blob/0df3279999bc3a14a37a4ced4150ab1d4c894733/components/driver/ledc.c#L1017
        // https://github.com/espressif/esp-idf/blob/0df3279999bc3a14a37a4ced4150ab1d4c894733/components/driver/ledc.c#L782
        SetLeftScreenLightFade(leftDutyCycle, fadeMs);
        SetRightScreenLightFade(rightDutyCycle, fadeMs);
    } else {
        SetLeftScreenLight(leftDutyCycle);
        SetRightScreenLight(rightDutyCycle);
    }
}
