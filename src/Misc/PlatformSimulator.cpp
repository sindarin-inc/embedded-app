#if SIMULATOR

#include <string>
#include <chrono>
#include <freertos/FreeRTOS.h>

#include "Platform.hpp"
#include "sindarin-debug.h"

int LightingSetting;

void InitializePlatform() {}

auto GetBatteryVoltageLeft() -> float { return 0; }

auto GetBatteryVoltageRight() -> float { return 0; }

void SetCpuSpeedMhz(int speedMhz) {}

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

auto GetRunTimeMs() -> uint32_t { return millis(); }

auto USBConnected() -> bool { return true; }

auto GetSerialNumber() -> std::string { return "SIMULATOR"; }

auto GetHardwareRevision() -> uint8_t { return 0; }

void ForcePowerOn() {}

void AllowPowerOff() {}


extern "C" void vTaskDelay(const TickType_t xTicksToDelay);

std::chrono::system_clock::time_point SimStartTime;

void hardwareInit() { SimStartTime = std::chrono::system_clock::now(); }

void delay(int delay_ms) { vTaskDelay(delay_ms / portTICK_PERIOD_MS); }

int millis() {
    auto duration = std::chrono::system_clock::now() - SimStartTime;
    auto durationMs = std::chrono::duration_cast<std::chrono::milliseconds>(duration);
    return (int)durationMs.count();
}

void digitalWrite(int pin, int level) {
    log_i("digitalWrite: pin %d is level %d", pin, level);
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
