#include "esp_sleep.h"

esp_err_t esp_sleep_enable_timer_wakeup(uint64_t time_in_us) { return ESP_OK; }

void esp_deep_sleep_start(){};

esp_sleep_wakeup_cause_t esp_sleep_get_wakeup_cause(void) { return ESP_SLEEP_WAKEUP_UNDEFINED; }
