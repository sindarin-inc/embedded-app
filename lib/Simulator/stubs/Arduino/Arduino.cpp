#include "Arduino.h"

#include <chrono>
#include <esp_core_dump.h>
#include <esp_flash.h>
#include <freertos/FreeRTOS.h>
#include <iostream>

extern "C" void vTaskDelay(const TickType_t xTicksToDelay);

void delay(int delay_ms) { vTaskDelay(delay_ms / portTICK_PERIOD_MS); }

void digitalWrite(int, int){};

std::chrono::system_clock::time_point ArduinoStartTime;

unsigned long millis() {
    auto duration = std::chrono::system_clock::now() - ArduinoStartTime;
    auto durationMs = std::chrono::duration_cast<std::chrono::milliseconds>(duration);
    return durationMs.count();
}

void initArduino() { ArduinoStartTime = std::chrono::system_clock::now(); }
