#pragma once
#include <Arduino.h>

void InitializePlatform();

auto GetBatteryVoltageLeft() -> float;

auto GetBatteryVoltageRight() -> float;
