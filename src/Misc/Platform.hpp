#pragma once

#include <cstdint>
#include <string>

// Note: This abstraction exists as an abstraction layer between the specific hardware and the rest
// of the firmware. It exists to make running on the simulator and other processors simpler by
// unifying some of the platform-specific code.

#define LOW 0
#define HIGH 1

//GPIO FUNCTIONS
#define INPUT             0x01
#define OUTPUT            0x02
#define PULLUP            0x04
#define INPUT_PULLUP      0x05
#define PULLDOWN          0x08
#define INPUT_PULLDOWN    0x09
#define OPEN_DRAIN        0x10
#define OUTPUT_OPEN_DRAIN 0x12
#define ANALOG            0xC0

#define MALLOC_CAP_SPIRAM           (1<<10) ///< Memory must be in SPI RAM

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

int digitalRead(uint8_t pin);

void delay(int ms);

int millis();