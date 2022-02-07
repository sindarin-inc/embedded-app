#ifndef SINDARIN_DEBUG_H
#define SINDARIN_DEBUG_H

#include <Arduino.h>

#define DEBUG_VAR(x) log_d("%s", String(x).c_str())
#define DEBUG_LN(y) log_d("%s", String(y).c_str())

#if SIMULATOR
#define SHOW_HEAP()
#else
#define SHOW_HEAP()                                                                                \
    log_d("Free heap: %dk/%dk Free PSRAM: %dk/%dk", ESP.getFreeHeap() / 1000,                      \
          ESP.getHeapSize() / 1000, ESP.getFreePsram() / 1000, ESP.getPsramSize() / 1000)
#endif

#define SIZE_OF(obj) log_d("SIZE_OF(%s): %zu", #obj, sizeof(obj))

// NOTE: based on the uint32_t, this cannot profile anything longer than 17.89569707
#define PROFILE_START(identifier)                                                                  \
    volatile uint32_t profileStart##identifier = 0;                                                \
    RSR(CCOUNT, profileStart##identifier);

#define PROFILE_END(identifier)                                                                    \
    volatile uint32_t profileEnd##identifier = 0;                                                  \
    RSR(CCOUNT, profileEnd##identifier);

// Calculates in ns the difference between PROFILE_START and PROFILE_END
#define PROFILE_CALCULATE_NS(identifier)                                                           \
    (((profileEnd##identifier - profileStart##identifier) * 1000.0f) /                             \
     (esp_clk_cpu_freq() / 1000000.0f))

// Logs the result of the profile
#define PROFILE_LOG(identifier)                                                                    \
    log_d("PROFILE_LOG(" #identifier ") %.0fns (%d-%d=%d cycles)",                                 \
          PROFILE_CALCULATE_NS(identifier), profileEnd##identifier, profileStart##identifier,      \
          profileEnd##identifier - profileStart##identifier)

// Convenience method that ends the profile then logs the result
#define PROFILE_END_LOG(identifier)                                                                \
    PROFILE_END(identifier);                                                                       \
    PROFILE_LOG(identifier);

#endif
