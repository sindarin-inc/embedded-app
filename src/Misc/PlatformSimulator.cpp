#if SIMULATOR

#include <string>

#include "Platform.hpp"
#include "sindarin-debug.h"

extern int LightingSetting;

void InitializePlatform() {}

auto GetBatteryVoltageLeft() -> float { return 0; }

auto GetBatteryVoltageRight() -> float { return 0; }

void SetCpuSpeedMhz(int speedMhz) {}

void SetBacklight(int percent) {
    LightingSetting = percent;
    log_d("Setting backlight to %d%%", percent);
}

void SetRightBackLight(int percent) {
    log_d("Setting right backlight to %d%% (but ignoring b/c Simulator)", percent);
}

void SetLeftBackLight(int percent) {
    LightingSetting = percent;
    log_d("Setting left backlight to %d%%", percent);
}

void SetLeftScreenLight(int dutyCycle) {
    LightingSetting = percent;
    log_d("Setting backlight to dutyCycle=%d", dutyCycle);
}

void SetLeftScreenLightFade(int dutyCycle, int fadeMs) {
    LightingSetting = percent;
    log_d("Fading backlight to dutyCycle=%d", dutyCycle);
}

void SetRightScreenLight(int dutyCycle) {
    log_d("Setting right backlight to dutyCycle=%d (but ignoring b/c Simulator)", percent);
}

void SetRightScreenLightFade(int dutyCycle, int fadeMs) {
    log_d("Fading right backlight to dutyCycle=%d (but ignoring b/c Simulator)", percent);
}

auto GetRunTimeMs() -> uint32_t { return millis(); }

auto USBConnected() -> bool { return true; }

auto GetSerialNumber() -> std::string { return "SIMULATOR"; }

auto GetHardwareRevision() -> uint8_t { return 0; }

#endif
