#pragma once

#include <cstdint>
#include <esp_err.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <string>

// Note: This abstraction exists as an abstraction layer between the specific hardware and the rest
// of the firmware. It exists to make running on the simulator and other processors simpler by
// unifying some of the platform-specific code.

#define LOW 0
#define HIGH 1

// GPIO FUNCTIONS
#define INPUT 0x01
#define OUTPUT 0x02
#define PULLUP 0x04
#define INPUT_PULLUP 0x05
#define PULLDOWN 0x08
#define INPUT_PULLDOWN 0x09
#define OPEN_DRAIN 0x10
#define OUTPUT_OPEN_DRAIN 0x12
#define ANALOG 0xC0

// Interrupt Modes
#define RISING 0x01
#define FALLING 0x02
#define CHANGE 0x03
#define ONLOW 0x04
#define ONHIGH 0x05
#define ONLOW_WE 0x0C
#define ONHIGH_WE 0x0D

#define ENCRYPTED_BLOCK_SIZE 16

#define U_FLASH 0

#define digitalPinToInterrupt(p) (((p) < 48) ? (p) : -1)

#define ESP_IMAGE_HEADER_MAGIC 0xE9 /*!< The magic word for the esp_image_header_t structure. */

#define FACTORY_RESET_BUTTON 0
#define FACTORY_RESET_MS 5000

// Bare minimum init before processing wake reasons
void HardwareInit();

auto HardwareDeinit() -> esp_err_t;

// Full initialization of all peripherals needed in active mode
void InitializePlatform();

auto DeinitializePlatform() -> esp_err_t;

auto GetUartCommandQueue() -> QueueHandle_t;

auto GetBatteryVoltageLeft() -> float;

auto GetBatteryVoltageRight() -> float;

void SetLeftScreenLight(int dutyCycle);

void SetLeftScreenLightFade(int dutyCycle, int fadeMs);

void SetRightScreenLight(int dutyCycle);

void SetRightScreenLightFade(int dutyCycle, int fadeMs);

auto GetMinLightingDutyCycle() -> uint8_t;

// For when the user wants to override the value manually
auto GetMinLightingDutyCycleOverride() -> uint8_t;

auto GetMaxLightingDutyCycle() -> uint8_t;

auto GetRunTimeMs() -> uint32_t;

auto USBConnected() -> bool;

auto GetSerialNumber() -> std::string;

auto GetMacAddress() -> std::string;

auto GetHardwareRevision() -> uint8_t;

void ForcePowerOn();

void AllowPowerOff();

void Delay(int ms);

auto Millis() -> int;

void DigitalWrite(int pin, int level);

auto DigitalRead(uint8_t pin) -> int;

#if !SIMULATOR
void PinMode(int pin, int mode);
auto GetHeapSize() -> size_t;
void EnableCore0Wdt();
void DisableCore0Wdt();
#else
uint32_t esp_random(void);
#endif
