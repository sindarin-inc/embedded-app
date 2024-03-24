#if SIMULATOR

#include <chrono>
#include <cstring>
#include <freertos/FreeRTOS.h>
#include <string>

#include "Platform.hpp"
#include "sindarin-debug.h"

int LightingSetting;
bool ledChange;
bool simLeds[3];

auto HardwareDeinit() -> esp_err_t { return ESP_OK; }

void InitializePlatform() {
    for (int i = 0; i < 3; i++) {
        simLeds[i] = false;
    }
    ledChange = false;
}

auto DeinitializePlatform() -> esp_err_t { return ESP_OK; }

auto GetUartCommandQueue() -> QueueHandle_t { return nullptr; }

auto GetBatteryVoltageLeft() -> float { return 0; }

auto GetBatteryVoltageRight() -> float { return 0; }

void SetLeftScreenLight(int dutyCycle) {
    LightingSetting = dutyCycle;
    // log_d("Setting screen light to dutyCycle=%d", dutyCycle);
}

void SetLeftScreenLightFade(int dutyCycle, int fadeMs) {
    LightingSetting = dutyCycle;
    // log_d("Fading screen light to dutyCycle=%d", dutyCycle);
}

void SetRightScreenLight(int dutyCycle) {
    // log_d("Setting right screen light to dutyCycle=%d (but ignoring b/c Simulator)", dutyCycle);
}

void SetRightScreenLightFade(int dutyCycle, int fadeMs) {
    // log_d("Fading right screen light to dutyCycle=%d (but ignoring b/c Simulator)", dutyCycle);
}

auto GetMinLightingDutyCycle() -> uint8_t { return 10; }

auto GetMinLightingDutyCycleOverride() -> uint8_t { return 1; }

auto GetMaxLightingDutyCycle() -> uint8_t { return 210; }

auto GetRunTimeMs() -> uint32_t { return Millis(); }

auto USBConnected() -> bool { return true; }

auto GetSerialNumber() -> std::string { return "SIMULATOR"; }

auto GetMacAddress() -> std::string { return "SIMULATOR"; }

auto GetHardwareRevision() -> uint8_t { return 0; }

void ForcePowerOn() {}

void AllowPowerOff() {}

extern "C" void vTaskDelay(const TickType_t xTicksToDelay);

std::chrono::system_clock::time_point SimStartTime;

void HardwareInit() { SimStartTime = std::chrono::system_clock::now(); }

void Delay(int delay_ms) { vTaskDelay(delay_ms / portTICK_PERIOD_MS); }

int Millis() {
    auto duration = std::chrono::system_clock::now() - SimStartTime;
    auto durationMs = std::chrono::duration_cast<std::chrono::milliseconds>(duration);
    return (int)durationMs.count();
}

void DigitalWrite(int pin, int level) {
    bool oldSimLeds[3];
    memcpy(oldSimLeds, simLeds, sizeof(oldSimLeds));

    if (pin == RED_LED) {
        if (level == LED_OFF) {
            simLeds[0] = false;
        } else {
            simLeds[0] = true;
        }
    } else if (pin == GREEN_LED) {
        if (level == LED_OFF) {
            simLeds[1] = false;
        } else {
            simLeds[1] = true;
        }
    } else if (pin == BLUE_LED) {
        if (level == LED_OFF) {
            simLeds[2] = false;
        } else {
            simLeds[2] = true;
        }
    }

    // Detect change so we only rerender when there's a change
    for (int i = 0; i < 3; i++) {
        if (oldSimLeds[i] != simLeds[i]) {
            ledChange = true;
        }
    }
}

// mock for reading GPIO, default to logic high
auto DigitalRead(uint8_t pin) -> int { return 1; }

// for logging
const char *pathToFileName(const char *path) {
    size_t i = 0;
    size_t pos = 0;
    char *p = (char *)path;
    while (*p) {
        i++;
        if (*p == '/' || *p == '\\') {
            pos = i;
        }
        p++;
    }
    return path + pos;
}

uint32_t esp_random(void) { return (uint32_t)rand(); }

#endif
