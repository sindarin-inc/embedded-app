#include "esp_sleep.h"

esp_err_t esp_sleep_enable_timer_wakeup(uint64_t time_in_us) { return ESP_OK; }

void esp_deep_sleep_start(){};

esp_sleep_wakeup_cause_t esp_sleep_get_wakeup_cause(void) { return ESP_SLEEP_WAKEUP_UNDEFINED; }

esp_err_t esp_sleep_enable_ext0_wakeup(gpio_num_t gpio_num, int level) { return ESP_OK; }

esp_err_t esp_sleep_disable_wakeup_source(esp_sleep_source_t source) { return ESP_OK; }