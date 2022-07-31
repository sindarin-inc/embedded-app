#ifndef SINDARIN_DEBUG_H
#define SINDARIN_DEBUG_H

#include <Arduino.h>

#if SIMULATOR

#define SHOW_HEAP()
#define PROFILE_START(identifier)
#define PROFILE_END(identifier)
#define PROFILE_CALCULATE_MS(identifier)
#define PROFILE_CALCULATE_US(identifier)
#define PROFILE_CALCULATE_NS(identifier)
#define PROFILE_LOG_MS(identifier)
#define PROFILE_LOG_US(identifier)
#define PROFILE_LOG_NS(identifier)
#define PROFILE_END_LOG_MS(identifier)
#define PROFILE_END_LOG_US(identifier)
#define PROFILE_END_LOG_NS(identifier)

#else
extern "C" {
#include <esp32/clk.h>
}

// For IDF builds, use the ESP_LOGD macros since they work with power management.
#ifdef IDF_LOG
#undef log_v
#undef log_d
#undef log_i
#undef log_w
#undef log_e
#define log_v(format, ...) ESP_LOGV("Sindarin", format, ##__VA_ARGS__);
#define log_d(format, ...) ESP_LOGD("Sindarin", format, ##__VA_ARGS__);
#define log_i(format, ...) ESP_LOGI("Sindarin", format, ##__VA_ARGS__);
#define log_w(format, ...) ESP_LOGW("Sindarin", format, ##__VA_ARGS__);
#define log_e(format, ...) ESP_LOGE("Sindarin", format, ##__VA_ARGS__);
#endif

#define SHOW_HEAP()                                                                                \
    log_d("Free heap: %dk/%dk Free PSRAM: %dk/%dk", ESP.getFreeHeap() / 1000,                      \
          ESP.getHeapSize() / 1000, heap_caps_get_free_size(MALLOC_CAP_8BIT) / 1000,               \
          heap_caps_get_total_size(MALLOC_CAP_8BIT) / 1000)

// NOTE: based on the uint32_t, this cannot profile anything longer than 17.89569707
#define PROFILE_START(identifier)                                                                  \
    volatile uint32_t profileStart##identifier = 0;                                                \
    RSR(CCOUNT, profileStart##identifier);

#define PROFILE_END(identifier)                                                                    \
    volatile uint32_t profileEnd##identifier = 0;                                                  \
    RSR(CCOUNT, profileEnd##identifier);

#define CYCLES_TO_MS(cycles) (((cycles)*1000.0f) / (esp_clk_cpu_freq()))

#define CYCLES_TO_US(cycles) (((cycles)*1000.0f) / (esp_clk_cpu_freq() / 1000.0f))

#define CYCLES_TO_NS(cycles) (((cycles)*1000.0f) / (esp_clk_cpu_freq() / 1000000.0f))

#define PROFILE_CALCULATE_MS(identifier)                                                           \
    CYCLES_TO_MS(profileEnd##identifier - profileStart##identifier)

// Calculates in us the difference between PROFILE_START and PROFILE_END
#define PROFILE_CALCULATE_US(identifier)                                                           \
    CYCLES_TO_US(profileEnd##identifier - profileStart##identifier)

// Calculates in ns the difference between PROFILE_START and PROFILE_END
#define PROFILE_CALCULATE_NS(identifier)                                                           \
    CYCLES_TO_NS(profileEnd##identifier - profileStart##identifier)

#define PROFILE_LOG_MS(identifier)                                                                 \
    log_d("PROFILE_LOG_MS(" #identifier ") %.0fms (%u-%u=%u cycles)",                              \
          PROFILE_CALCULATE_MS(identifier), profileEnd##identifier, profileStart##identifier,      \
          profileEnd##identifier - profileStart##identifier)

#define PROFILE_LOG_US(identifier)                                                                 \
    log_d("PROFILE_LOG_US(" #identifier ") %.0fus (%u-%u=%u cycles)",                              \
          PROFILE_CALCULATE_US(identifier), profileEnd##identifier, profileStart##identifier,      \
          profileEnd##identifier - profileStart##identifier)

// Logs the result of the profile
#define PROFILE_LOG_NS(identifier)                                                                 \
    log_d("PROFILE_LOG_NS(" #identifier ") %.0fns (%u-%u=%u cycles)",                              \
          PROFILE_CALCULATE_NS(identifier), profileEnd##identifier, profileStart##identifier,      \
          profileEnd##identifier - profileStart##identifier)

#define PROFILE_END_LOG_MS(identifier)                                                             \
    PROFILE_END(identifier);                                                                       \
    PROFILE_LOG_MS(identifier);

#define PROFILE_END_LOG_US(identifier)                                                             \
    PROFILE_END(identifier);                                                                       \
    PROFILE_LOG_US(identifier);

// Convenience method that ends the profile then logs the result
#define PROFILE_END_LOG_NS(identifier)                                                             \
    PROFILE_END(identifier);                                                                       \
    PROFILE_LOG_NS(identifier);

#endif // SIMULATOR

#define SIZE_OF(obj) log_d("SIZE_OF(%s): %zu", #obj, sizeof(obj))

#endif
