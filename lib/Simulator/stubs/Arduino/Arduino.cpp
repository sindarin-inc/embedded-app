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

esp_err_t esp_flash_read(void *chip, void *buffer, uint32_t address, uint32_t length) {
    // test
    memset(buffer, 'A', length);
    return ESP_OK;    
}

esp_err_t esp_core_dump_image_get(size_t* out_addr, size_t *out_size) {
    *out_size = 128;
    return ESP_OK;
}