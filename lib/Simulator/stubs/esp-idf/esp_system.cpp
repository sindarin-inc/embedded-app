#include "esp_system.h"

esp_reset_reason_t esp_reset_reason(void) { return ESP_RST_SW; }

void esp_restart() { exit(0); }
