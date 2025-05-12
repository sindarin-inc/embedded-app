#pragma once

#ifndef DEBUG_MEMORY
#define DEBUG_MEMORY 0
#endif

#if SIMULATOR

#include <assert.h>
#include <chrono>
#include <inttypes.h>
#include <stdio.h>

#define PROFILE_START(identifier)                                                                  \
    auto profileStart##identifier = std::chrono::high_resolution_clock::now();

#define PROFILE_END(identifier)                                                                    \
    auto profileEnd##identifier = std::chrono::high_resolution_clock::now();

#define CYCLES_TO_MS(cycles) 0.0f

#define CYCLES_TO_US(cycles) 0.0f

#define CYCLES_TO_NS(cycles) 0.0f

#define PROFILE_CALCULATE_MS(identifier) 0

#define PROFILE_CALCULATE_US(identifier) 0

#define PROFILE_CALCULATE_NS(identifier) 0

#define PROFILE_LOG_MS(identifier)                                                                 \
    log_d("PROFILE_LOG_MS(" #identifier ") %" PRIu64 "ms",                                         \
          std::chrono::duration_cast<std::chrono::milliseconds>(profileEnd##identifier -           \
                                                                profileStart##identifier)          \
              .count());

#define PROFILE_LOG_US(identifier)                                                                 \
    log_d("PROFILE_LOG_US(" #identifier ") %" PRIu64 "us",                                         \
          std::chrono::duration_cast<std::chrono::microseconds>(profileEnd##identifier -           \
                                                                profileStart##identifier)          \
              .count());

// Logs the result of the profile
#define PROFILE_LOG_NS(identifier)                                                                 \
    log_d("PROFILE_LOG_US(" #identifier ") %" PRIu64 "ns",                                         \
          std::chrono::duration_cast<std::chrono::nanoseconds>(profileEnd##identifier -            \
                                                               profileStart##identifier)           \
              .count());

#define PROFILE_MULTIPLE_SETUP(identifier)

#define PROFILE_MULTIPLE_START(identifier)

#define PROFILE_MULTIPLE_END(identifier)

#define PROFILE_MULTIPLE_LOG(identifier)

const char *pathToFileName(const char *path);

#define SIM_LOG_COLOR_BLACK "30"
#define SIM_LOG_COLOR_RED "31"    // ERROR
#define SIM_LOG_COLOR_GREEN "32"  // INFO
#define SIM_LOG_COLOR_YELLOW "33" // WARNING
#define SIM_LOG_COLOR_BLUE "34"
#define SIM_LOG_COLOR_MAGENTA "35"
#define SIM_LOG_COLOR_CYAN "36" // DEBUG
#define SIM_LOG_COLOR_GRAY "37" // VERBOSE
#define SIM_LOG_COLOR_WHITE "38"

#define SIM_LOG_COLOR(COLOR) "\033[0;" COLOR "m"
#define SIM_LOG_BOLD(COLOR) "\033[1;" COLOR "m"
#define SIM_LOG_RESET_COLOR "\033[0m"

#define SIM_LOG_COLOR_E SIM_LOG_COLOR(SIM_LOG_COLOR_RED)
#define SIM_LOG_COLOR_W SIM_LOG_COLOR(SIM_LOG_COLOR_YELLOW)
#define SIM_LOG_COLOR_I SIM_LOG_COLOR(SIM_LOG_COLOR_GREEN)
#define SIM_LOG_COLOR_D SIM_LOG_COLOR(SIM_LOG_COLOR_CYAN)
#define SIM_LOG_COLOR_V SIM_LOG_COLOR(SIM_LOG_COLOR_GRAY)

#define log_printf(fmt, ...)                                                                       \
    do {                                                                                           \
        auto log_now = std::chrono::system_clock::now();                                           \
        auto log_now_time_t = std::chrono::system_clock::to_time_t(log_now);                       \
        auto log_now_ms =                                                                          \
            std::chrono::duration_cast<std::chrono::milliseconds>(log_now.time_since_epoch()) %    \
            1000;                                                                                  \
        char log_time_str[32];                                                                     \
        strftime(log_time_str, sizeof(log_time_str), "%H:%M:%S", localtime(&log_now_time_t));      \
        fprintf(stdout, "[%s.%03" PRIu64 "] ", log_time_str, log_now_ms.count());                  \
        fprintf(stdout, fmt, ##__VA_ARGS__);                                                       \
        fflush(stdout);                                                                            \
    } while (0)
#define SIM_LOG_FORMAT(letter, format)                                                             \
    SIM_LOG_COLOR_##letter "[" #letter "][%s:%u] %s(): " format SIM_LOG_RESET_COLOR "\r\n",        \
        pathToFileName(__FILE__), __LINE__, __FUNCTION__

#define log_v(format, ...) log_printf(SIM_LOG_FORMAT(V, format), ##__VA_ARGS__)
#define log_d(format, ...) log_printf(SIM_LOG_FORMAT(D, format), ##__VA_ARGS__)
#define log_i(format, ...) log_printf(SIM_LOG_FORMAT(I, format), ##__VA_ARGS__)
#define log_w(format, ...) log_printf(SIM_LOG_FORMAT(W, format), ##__VA_ARGS__)
#define log_e(format, ...) log_printf(SIM_LOG_FORMAT(E, format), ##__VA_ARGS__)

#else
// esp_clk methods are deprecated in IDF 5+
// TODO: replace
extern "C" {
#include <esp_private/esp_clk.h>
}

#include <esp_log.h>
#if __XTENSA__
#include <xt_instr_macros.h>
#endif
#include "Misc/Platform.hpp"

// For IDF builds, use the ESP_LOGD macros since they work with power management.
#ifdef IDF_LOG
#undef log_v
#undef log_d
#undef log_i
#undef log_w
#undef log_e
#define log_v(format, ...) ESP_LOGV("Sol", format, ##__VA_ARGS__);
#define log_d(format, ...) ESP_LOGD("Sol", format, ##__VA_ARGS__);
#define log_i(format, ...) ESP_LOGI("Sol", format, ##__VA_ARGS__);
#define log_w(format, ...) ESP_LOGW("Sol", format, ##__VA_ARGS__);
#define log_e(format, ...) ESP_LOGE("Sol", format, ##__VA_ARGS__);
#endif

inline auto GetCycleCount() -> uint64_t {
    volatile uint32_t cycle;
#if __XTENSA__
    RSR(CCOUNT, cycle);
#else
    asm volatile("rdcycle %0" : "=r"(cycle));
#endif
    return cycle;
}

// NOTE: based on the uint32_t, this cannot profile anything longer than 17.89569707
#define PROFILE_START(identifier) volatile uint32_t profileStart##identifier = GetCycleCount();

#define PROFILE_END(identifier) volatile uint32_t profileEnd##identifier = GetCycleCount();

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
    log_d("PROFILE_LOG_MS(" #identifier ") %.0fms (%" PRIu32 "-%" PRIu32 "=%" PRIu32 " cycles)",   \
          PROFILE_CALCULATE_MS(identifier), profileEnd##identifier, profileStart##identifier,      \
          profileEnd##identifier - profileStart##identifier)

#define PROFILE_LOG_US(identifier)                                                                 \
    log_d("PROFILE_LOG_US(" #identifier ") %.0fus (%" PRIu32 "-%" PRIu32 "=%" PRIu32 " cycles)",   \
          PROFILE_CALCULATE_US(identifier), profileEnd##identifier, profileStart##identifier,      \
          profileEnd##identifier - profileStart##identifier)

// Logs the result of the profile
#define PROFILE_LOG_NS(identifier)                                                                 \
    log_d("PROFILE_LOG_NS(" #identifier ") %.0fns (%" PRIu32 "-%" PRIu32 "=%" PRIu32 " cycles)",   \
          PROFILE_CALCULATE_NS(identifier), profileEnd##identifier, profileStart##identifier,      \
          profileEnd##identifier - profileStart##identifier)

#define PROFILE_MULTIPLE_SETUP(identifier)                                                         \
    uint32_t profileCount##identifier = 0;                                                         \
    uint32_t profileTotalCycles##identifier = 0;                                                   \
    uint32_t profileMaxCycles##identifier = 0;                                                     \
    uint32_t profileMinCycles##identifier = UINT32_MAX;

#define PROFILE_MULTIPLE_START(identifier)                                                         \
    volatile uint32_t profileStart##identifier = GetCycleCount();

#define PROFILE_MULTIPLE_END(identifier)                                                           \
    volatile uint32_t profileEnd##identifier = GetCycleCount();                                    \
    profileCount##identifier += 1;                                                                 \
    uint32_t profileCycles##identifier = profileEnd##identifier - profileStart##identifier;        \
    profileTotalCycles##identifier += profileCycles##identifier;                                   \
    if (profileCycles##identifier > profileMaxCycles##identifier) {                                \
        profileMaxCycles##identifier = profileCycles##identifier;                                  \
    }                                                                                              \
    if (profileCycles##identifier < profileMinCycles##identifier) {                                \
        profileMinCycles##identifier = profileCycles##identifier;                                  \
    }

#define PROFILE_MULTIPLE_LOG(identifier)                                                           \
    log_d("PROFILE_MULTIPLE_LOG(" #identifier ") %.02fus/iteration (%.02fus / %" PRIu32            \
          ") (max: %.02fus, min: %.02fus)",                                                        \
          CYCLES_TO_US(profileTotalCycles##identifier / profileCount##identifier),                 \
          CYCLES_TO_US(profileTotalCycles##identifier), profileCount##identifier,                  \
          CYCLES_TO_US(profileMaxCycles##identifier), CYCLES_TO_US(profileMinCycles##identifier));

#endif // SIMULATOR

// Convenience method that ends the profile then logs the result
#define PROFILE_END_LOG_MS(identifier)                                                             \
    PROFILE_END(identifier);                                                                       \
    PROFILE_LOG_MS(identifier);

#define PROFILE_END_LOG_US(identifier)                                                             \
    PROFILE_END(identifier);                                                                       \
    PROFILE_LOG_US(identifier);

#define PROFILE_END_LOG_NS(identifier)                                                             \
    PROFILE_END(identifier);                                                                       \
    PROFILE_LOG_NS(identifier);

#define SIZE_OF(obj) log_d("SIZE_OF(%s): %zu", #obj, sizeof(obj))

#if DEBUG_MEMORY

#define SHOW_HEAP()                                                                                \
    log_d("Free heap: %zuk/%zuk Free PSRAM: %zuk/%zuk %s:%d ",                                     \
          heap_caps_get_free_size(MALLOC_CAP_INTERNAL) / 1000, GetHeapSize() / 1000,               \
          heap_caps_get_free_size(MALLOC_CAP_8BIT) / 1000,                                         \
          heap_caps_get_total_size(MALLOC_CAP_8BIT) / 1000, __FILE__, __LINE__)

#define HEAP_PROFILE_START(identifier)                                                             \
    size_t heapStart##identifier = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);

#define HEAP_PROFILE_END(identifier)                                                               \
    size_t heapEnd##identifier = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);                     \
    log_d("HEAP_PROFILE_END(" #identifier ") %d alloc'd (%zu-%zu) %s:%d",                          \
          static_cast<int>(heapStart##identifier) - static_cast<int>(heapEnd##identifier),         \
          heapEnd##identifier, heapStart##identifier, __FILE__, __LINE__);

#define SHOW_WATERMARK()                                                                           \
    log_d("SHOW_WATERMARK(%s) (min remaining stack): %zu %s:%d", pcTaskGetName(nullptr),           \
          uxTaskGetStackHighWaterMark(NULL), __FILE__, __LINE__);

#else

#define SHOW_HEAP()
#define HEAP_PROFILE_START(identifier)
#define HEAP_PROFILE_END(identifier)
#define SHOW_WATERMARK()

#endif
