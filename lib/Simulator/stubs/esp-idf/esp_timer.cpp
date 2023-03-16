#include "esp_timer.h"

#include <chrono>

// Comes from the Arduino stub library and is set in initArduino()
extern std::chrono::system_clock::time_point SimStartTime;

int64_t esp_timer_get_time(void) {
    auto duration = std::chrono::system_clock::now() - SimStartTime;
    auto durationUs = std::chrono::duration_cast<std::chrono::microseconds>(duration);
    return durationUs.count();
}
