#include "esp_log.h"

#include <stdio.h>
#include <time.h>

void esp_log_level_set(const char *tag, esp_log_level_t level) { return; }

esp_log_level_t esp_log_level_get(const char *tag) { return ESP_LOG_DEBUG; }

uint32_t esp_log_timestamp(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint32_t)(ts.tv_sec * 1000 + ts.tv_nsec / 1000000);
}

void esp_log_write(esp_log_level_t level, const char *tag, const char *format, ...) {
    va_list args;
    va_start(args, format);
    char buffer[512];
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    fprintf(stdout, "%s", buffer);
}
