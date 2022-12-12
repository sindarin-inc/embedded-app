#include "LightingController.hpp"

#include <driver/ledc.h>

#include "Misc/Platform.hpp"
#include "sindarin-debug.h"

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
    currentSetting_ = 0;
    setLighting(currentSetting_, 200);
}

void LightingController::enableLighting() {
    if (!lightsOut_) {
        return;
    }
    lightsOut_ = false;
    if (overrideValue_ != -1) {
        currentSetting_ = overrideValue_;
        setLighting(currentSetting_, 200);
        return;
    } else {
        lightSensor_.enable();
    }
}

void LightingController::setOverrideValue(int percent) {
    overrideValue_ = percent;
    lightSensor_.disable();
    currentSetting_ = percent;
    setLighting(currentSetting_, 100);
}

void LightingController::setScreenMask(Screens screens, int fadeMs) {
    bool needsUpdate = screens_ != screens;
    screens_ = screens;
    setLighting(currentSetting_, fadeMs);
};

void LightingController::checkAmbientLight(int fadeMs, int timeoutMs) {
    if (lightsOut_) {
        return;
    }
    if (overrideValue_ != -1) {
        return;
    }

    uint16_t channel1, channel0;
    lightSensor_.readData(channel1, channel0);
    if (channel1 == 0) {
        // If it's zero, check that the data is actually valid and wait for valid data if needed
        // Note(john): 0 is a valid value here, so we're possibly doing some extra work here when in
        // total darkness, but the alternative is to _always_ do extra work so I think this is
        // better
        ltr329als01_status_value status;
        auto startTime = millis();
        while (true) {
            lightSensor_.readStatus(status);
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

    movingAverage_.add(std::min(channel1, uint16_t(1000)));
    auto average = movingAverage_.average();
    currentSetting_ = std::clamp(average / 10, 2, 100);
    log_d("Light sensor ch1: %d ch0: %d average: %d setting: %d", channel1, channel0, average,
          currentSetting_);
    setLighting(currentSetting_, fadeMs);
}

void LightingController::setLighting(int percent, int fadeMs) {
    currentSetting_ = percent;
    auto dutyCycle = percent * 255 / 100;
    log_d("Setting backlight to %d%% (%d) (fadeMs=%d)", percent, dutyCycle, fadeMs);

    int leftDuty = 0;
    int rightDuty = 0;
    if (screens_ | LEFT) {
        leftDuty = dutyCycle;
    }
    if (screens_ | RIGHT) {
        rightDuty = dutyCycle;
    }

    esp_err_t err;
#if LEDC_BACKLIGHT
    if (fadeMs > 0) {
        // Set the LED to the target brightness
        err = ledc_set_fade_with_time(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, leftDuty, fadeMs);
        if (err != ESP_OK) {
            log_e("Error setting fade with time: %s", esp_err_to_name(err));
            return;
        }
        // Start the LED fading process
        err = ledc_fade_start(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, LEDC_FADE_NO_WAIT);
        if (err != ESP_OK) {
            log_e("Error starting fade: %s", esp_err_to_name(err));
            return;
        }
        // Set the LED to the target brightness
        err = ledc_set_fade_with_time(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_1, rightDuty, fadeMs);
        if (err != ESP_OK) {
            log_e("Error setting fade with time: %s", esp_err_to_name(err));
            return;
        }
        // Start the LED fading process
        err = ledc_fade_start(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_1, LEDC_FADE_NO_WAIT);
        if (err != ESP_OK) {
            log_e("Error starting fade: %s", esp_err_to_name(err));
            return;
        }
    } else {
        err = ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, leftDuty);
        if (err != ESP_OK) {
            log_e("Error setting duty: %s", esp_err_to_name(err));
            return;
        }
        err = ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);
        if (err != ESP_OK) {
            log_e("Error updating duty ch0: %s", esp_err_to_name(err));
            return;
        }
        err = ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_1, rightDuty);
        if (err != ESP_OK) {
            log_e("Error setting duty ch1: %s", esp_err_to_name(err));
            return;
        }
        err = ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_1);
        if (err != ESP_OK) {
            log_e("Error updating duty: %s", esp_err_to_name(err));
            return;
        }
    }
#endif
}
