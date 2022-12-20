#include "LightingController.hpp"

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "Misc/Math.hpp"
#include "Misc/Platform.hpp"
#include "sindarin-debug.h"

#define MIN_LIGHTING_DUTY_CYCLE 5
#define MAX_LIGHTING_DUTY_CYCLE 60

auto LightingController::checkSensorAvailable() -> bool {
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
    lightSensor_.enable();
}

void LightingController::disableLighting() {
    lightsOut_ = true;
    lightSensor_.disable();
    currentDutyCycle_ = 0;
    setCurrentDutyCycle(currentDutyCycle_, 200);
}

void LightingController::enableLighting() {
    if (!lightsOut_) {
        return;
    }
    lightsOut_ = false;
    if (overrideDutyCycle_ != -1) {
        currentDutyCycle_ = overrideDutyCycle_;
        setCurrentDutyCycle(currentDutyCycle_, 200);
        return;
    } else {
        lightSensor_.enable();
    }
}

void LightingController::setOverrideValuePercent(int percent) {
    auto dutyCycle = MapRange(percent, 0, 100, MIN_LIGHTING_DUTY_CYCLE, MAX_LIGHTING_DUTY_CYCLE);
    overrideDutyCycle_ = dutyCycle;
    lightSensor_.disable();
    currentDutyCycle_ = dutyCycle;
    setCurrentDutyCycle(currentDutyCycle_, 100);
}

auto LightingController::getCurrentPercent() -> int {
    return MapRange(currentDutyCycle_, MIN_LIGHTING_DUTY_CYCLE, MAX_LIGHTING_DUTY_CYCLE, 0, 100);
}

void LightingController::setScreenMask(Screens screens, int fadeMs) {
    log_d("Setting screens mask to %x", screens);
    screens_ = screens;
    setCurrentDutyCycle(currentDutyCycle_, fadeMs);
};

void LightingController::checkAmbientLight(int fadeMs, int timeoutMs) {
    if (lightsOut_) {
        return;
    }
    if (overrideDutyCycle_ != -1) {
        return;
    }

    uint16_t channel1, channel0;
    if (!lightSensor_.readData(channel1, channel0)) {
        // Failed to read data, so just give up and return (will be logged from the Ltr329Als
        // driver)
        return;
    }
    if (channel1 == 0) {
        // If it's zero, check that the data is actually valid and wait for valid data if needed
        // Note(john): 0 is a valid value here, so we're possibly doing some extra work here when in
        // total darkness, but the alternative is to _always_ do extra work so I think this is
        // better
        Ltr329als01StatusValue status;
        auto startTime = millis();
        while (true) {
            if (!lightSensor_.readStatus(status)) {
                // Failed to read status, so just give up and return (will be logged from the
                // Ltr329Als driver)
                return;
            }
            if (status.alsDataStatus == 1 && status.alsDataValid == 0) {
                break;
            }
            if (millis() - startTime > timeoutMs) {
                log_e("Light sensor timed out");
                return;
            }
            vTaskDelay(10 * portTICK_PERIOD_MS);
        };
        lightSensor_.readData(channel1, channel0);
    }
    // Clamp to a max value of 1000 to avoid the moving average getting too large
    movingAverage_.add(std::min(channel1, uint16_t(1000)));
    auto average = movingAverage_.average();

    currentDutyCycle_ =
        MapRange(average, 0, 1000, MIN_LIGHTING_DUTY_CYCLE, MAX_LIGHTING_DUTY_CYCLE);
    // log_d("Light sensor ch1: %d ch0: %d average: %d setting: %d", channel1, channel0, average,
    // currentDutyCycle_);
    setCurrentDutyCycle(currentDutyCycle_, fadeMs);
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
