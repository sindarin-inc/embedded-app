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
    // Not pictured here: all the LEDC configuration that happens in PlatformESP32.cpp. We keep that
    // code over there because the LEDC driver is configured globally and we'll probably also want
    // to configure the RGB LEDs with LEDC eventually
    if (!enabled_) {
        lightSensor_.enable();
        enabled_ = true;
    }
}

void LightingController::returnToDisabledState() {
    if (enabled_ && overrideDutyCycle_ != -1) {
        lightSensor_.disable();
        enabled_ = false;
    }
}

void LightingController::disableLighting(int fadeMs) {
    if (!enabled_) {
        return;
    }
    lightsOut_ = true;
    lightSensor_.disable();
    currentDutyCycle_ = 0;
    setCurrentDutyCycle(currentDutyCycle_, fadeMs);
    enabled_ = false;
}

void LightingController::enableLighting(int fadeMs) {
    if (!lightsOut_) {
        return;
    }
    lightsOut_ = false;
    if (overrideDutyCycle_ != -1) {
        currentDutyCycle_ = overrideDutyCycle_;
        setCurrentDutyCycle(currentDutyCycle_, fadeMs);
        return;
    } else {
        lightSensor_.enable();
        enabled_ = true;
    }
}

void LightingController::enableOnlyIncreaseMode() { onlyIncreaseMode_ = true; }
void LightingController::disableOnlyIncreaseMode() { onlyIncreaseMode_ = false; }

void LightingController::setOverrideValuePercent(int percent, bool maintainLightSensor) {
    auto dutyCycle = percent / 7;
    if (dutyCycle == 0) {
        dutyCycle = 1;
    } else if (dutyCycle == 1) {
        dutyCycle = 2;
    } else {
        dutyCycle *= dutyCycle;
    }

    overrideDutyCycle_ = dutyCycle;
    log_d("Override duty cycle: %d (%d%%)", dutyCycle, percent);
    if (!maintainLightSensor) {
        lightSensor_.disable();
        enabled_ = false;
    }
    currentDutyCycle_ = dutyCycle;
    setCurrentDutyCycle(currentDutyCycle_, 100);
}

auto LightingController::getCurrentPercent() const -> int {
    if (currentDutyCycle_ == 1) {
        return 0;
    } else if (currentDutyCycle_ == 2) {
        return 7;
    } else {
        return static_cast<int>(std::sqrt(currentDutyCycle_)) * 7;
    }
}

void LightingController::setScreenMask(Screens screens, int fadeMs) {
    log_d("Setting screens mask to %x", screens);
    screens_ = screens;
    setCurrentDutyCycle(currentDutyCycle_, fadeMs);
};

auto LightingController::readAmbientLightData(uint16_t &channel1, uint16_t &channel0) -> bool {
    if (!lightSensor_.readData(channel1, channel0)) {
        // Failed to read data, so just return false
        return false;
    }
    return true;
}
void LightingController::checkAmbientLight(int fadeMs, int timeoutMs) {
    if (lightsOut_) {
        log_i("Ambient light check requested but lights are out, ignoring");
        return;
    }
    if (overrideDutyCycle_ != -1) {
        return;
    }

    uint16_t channel1, channel0;
    if (!readAmbientLightData(channel1, channel0)) {
        // Failed to read data, so just give up and return (will be logged from the Ltr329Als
        // driver)
        log_w("Failed to read ambient light data from LTR329ALS");
        return;
    }

    // Clamp to a max value of 1000 to avoid the moving average getting too large
    movingAverage_.add(std::min(channel1, static_cast<uint16_t>(1000)));
    auto average = movingAverage_.average();

    int newDutyCycle = MapRange(average, 0, 1000, static_cast<int>(GetMinLightingDutyCycle()),
                                static_cast<int>(GetMaxLightingDutyCycle()));

    // If onlyIncreaseMode_ is enabled, only increase the duty cycle if the new value is greater
    // than current
    if (onlyIncreaseMode_ && newDutyCycle < currentDutyCycle_) {
        log_w("Only increase mode enabled, not decreasing duty cycle (new=%d, current=%d)",
              newDutyCycle, currentDutyCycle_);
        return;
    }

    currentDutyCycle_ = newDutyCycle;
    setCurrentDutyCycle(currentDutyCycle_, fadeMs);
    // log_w("Ambient light check: channel1=%d, channel0=%d, average=%d, dutyCycle=%d", channel1,
    //       channel0, average, currentDutyCycle_);
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
