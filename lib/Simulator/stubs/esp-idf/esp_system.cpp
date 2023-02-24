#include "esp_system.h"

esp_reset_reason_t esp_reset_reason(void) { return ESP_RST_SW; }

void esp_restart() {}

esp_err_t esp_sleep_enable_timer_wakeup(uint64_t time_in_us) { return ESP_OK; }