#pragma once

#include <cstdint>

void InitializePlatform();

auto GetBatteryVoltageLeft() -> float;

auto GetBatteryVoltageRight() -> float;

void SetCpuSpeedMhz(int speedMhz);

void SetBacklight(int percent);

uint32_t getRunTimeMs();
