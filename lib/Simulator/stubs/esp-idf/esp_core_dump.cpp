#include <esp_core_dump.h>

esp_err_t esp_core_dump_image_get(size_t* out_addr, size_t *out_size) {
    *out_size = 128;
    return ESP_OK;
}