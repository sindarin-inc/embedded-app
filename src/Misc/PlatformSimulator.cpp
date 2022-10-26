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

auto GetRunTimeMs() -> uint32_t { return millis(); }

auto USBConnected() -> bool { return true; }

auto GetSerialNumber() -> std::string { return "SIMULATOR"; }

#endif
