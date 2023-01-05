#if SIMULATOR

#include <string>

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

#endif
