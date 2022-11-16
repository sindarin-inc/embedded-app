#include "Arduino.h"
#include <esp_flash.h>
#include <esp_core_dump.h>

#include <SDL2/SDL.h>
#include <freertos/FreeRTOS.h>

extern "C" void vTaskDelay(const TickType_t xTicksToDelay);

void delay(int delay_ms) { vTaskDelay(delay_ms / portTICK_PERIOD_MS); }

void esp_deep_sleep_start(){};

void esp_restart(){};

void digitalWrite(int, int){};

unsigned long millis() { return SDL_GetTicks(); }

void initArduino() {}