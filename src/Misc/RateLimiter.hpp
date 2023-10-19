#include <esp_timer.h>
#include <functional>

class RateLimiter {
public:
    RateLimiter(int64_t minTime, int64_t maxTime, float minPercentage)
        : minTimeUs_(minTime * 1000000), maxTimeUs_(maxTime * 1000000),
          minPercentage_(minPercentage) {}

    void checkAndExecute(float percentage,
                         const std::function<void(int64_t time, float percentage)> &func) {
        auto now = esp_timer_get_time();
        if ((now - lastTime_ > minTimeUs_) &&
            (now - lastTime_ > maxTimeUs_ || (percentage - lastPercentage_) > minPercentage_)) {
            func(now, percentage);
            lastTime_ = now;
            lastPercentage_ = percentage;
        }
    }

private:
    int64_t minTimeUs_;
    int64_t maxTimeUs_;
    int minPercentage_;
    int64_t lastTime_{0};
    float lastPercentage_{0};
};
