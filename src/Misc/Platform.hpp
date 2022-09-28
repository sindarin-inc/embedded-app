#pragma once

#include <cstdint>

void InitializePlatform();

auto GetBatteryVoltageLeft() -> float;

auto GetBatteryVoltageRight() -> float;

void SetCpuSpeedMhz(int speedMhz);

void SetBacklight(int percent);

void SetRightBackLight(bool on, int percent);

void SetLeftBackLight(bool on, int percent);

auto GetRunTimeMs() -> uint32_t;

auto USBConnected() -> bool;