#if SIMULATOR

#include "Platform.hpp"
#include "sindarin-debug.h"

void InitializePlatform() {}

auto GetBatteryVoltageLeft() -> float { return 0; }

auto GetBatteryVoltageRight() -> float { return 0; }

void SetCpuSpeedMhz(int speedMhz) {}

void SetBacklight(int percent) { log_d("Setting backlight to %d%%", percent); }

uint32_t getRunTimeMs() { return millis(); }

#endif
