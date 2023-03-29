#if SIMULATOR

#include <chrono>
#include <freertos/FreeRTOS.h>
#include <string>

#include "Platform.hpp"
#include "sindarin-debug.h"

int LightingSetting;
bool ledChange;
bool simLeds[3];

void InitializePlatform() {
    for (int i = 0; i < 3; i++) {
        simLeds[i] = false;
    }
    ledChange = false;
}

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

auto GetRunTimeMs() -> uint32_t { return Millis(); }

auto USBConnected() -> bool { return true; }

auto GetSerialNumber() -> std::string { return "SIMULATOR"; }

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

    ledChange = true;
}

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

#endif
