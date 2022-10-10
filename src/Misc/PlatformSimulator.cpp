#if SIMULATOR

#include <string>

#include "Displays/DisplaySDLSimulator.hpp"
#include "Platform.hpp"
#include "sindarin-debug.h"

extern DisplaySDLSimulator myDisplay;

void InitializePlatform() {}

auto GetBatteryVoltageLeft() -> float { return 0; }

auto GetBatteryVoltageRight() -> float { return 0; }

void SetCpuSpeedMhz(int speedMhz) {}

void SetBacklight(int percent) {
    myDisplay.setLighting(percent);
    log_d("Setting backlight to %d%%", percent);
}

auto GetRunTimeMs() -> uint32_t { return millis(); }

auto USBConnected() -> bool { return true; }

auto GetSerialNumber() -> std::string { return "SIMULATOR"; }

#endif
