#pragma once

#define ESP_OK 0 /*!< esp_err_t value indicating success (no error) */

typedef int esp_err_t;

const char *esp_err_to_name(esp_err_t code);