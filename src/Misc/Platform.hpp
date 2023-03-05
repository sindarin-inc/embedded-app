#pragma once

#include <cstdint>
#include <string>

// Note: This abstraction exists as an abstraction layer between the specific hardware and the rest
// of the firmware. It exists to make running on the simulator and other processors simpler by
// unifying some of the platform-specific code.

#define LOW 0
#define HIGH 1

#define INPUT 0
#define OUTPUT 1


void InitializePlatform();

auto GetBatteryVoltageLeft() -> float;

auto GetBatteryVoltageRight() -> float;

void SetCpuSpeedMhz(int speedMhz);

void SetLeftScreenLight(int dutyCycle);

void SetLeftScreenLightFade(int dutyCycle, int fadeMs);

void SetRightScreenLight(int dutyCycle);

void SetRightScreenLightFade(int dutyCycle, int fadeMs);

auto GetRunTimeMs() -> uint32_t;

auto USBConnected() -> bool;

auto GetSerialNumber() -> std::string;

auto GetHardwareRevision() -> uint8_t;

void ForcePowerOn();

void AllowPowerOff();

void pinMode(int pin, int mode);

void digitalWrite(int pin, int level);

void delay(int ms);