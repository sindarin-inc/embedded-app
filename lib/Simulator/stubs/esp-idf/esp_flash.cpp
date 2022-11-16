#include <esp_flash.h>
#include <stdint.h>
#include <string.h>

esp_err_t esp_flash_read(void *chip, void *buffer, uint32_t address, uint32_t length) {
    // test
    memset(buffer, 'A', length);
    return ESP_OK;    
}