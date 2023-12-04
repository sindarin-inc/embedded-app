#include <esp_timer.h>
#include <functional>

class RateLimiter {
public:
    RateLimiter(int64_t minTime, int64_t maxTime, float minPercentage = 0.0f)
        : minTimeUs_(minTime * 1000000), maxTimeUs_(maxTime * 1000000),
          minPercentage_(minPercentage) {}

    auto shouldExecute(float percentage) -> bool {
        auto now = esp_timer_get_time();
        auto minTimeConditionMet = now - lastTime_ > minTimeUs_;
        auto maxTimeConditionMet = now - lastTime_ > maxTimeUs_;
        auto minPercentageConditionMet =
            (minPercentage_ == 0.0f) || (percentage - lastPercentage_ >= minPercentage_);
        auto shouldExecute =
            minTimeConditionMet && (maxTimeConditionMet || minPercentageConditionMet);
        if (shouldExecute) {
            lastTime_ = now;
            lastPercentage_ = percentage;
        }
        return shouldExecute;
    }

    void checkAndExecute(float percentage,
                         const std::function<void(int64_t time, float percentage)> &func) {
        if (shouldExecute(percentage)) {
            func(esp_timer_get_time(), percentage);
        }
    }

    void resetPercentage() { lastPercentage_ = 0; }

private:
    int64_t minTimeUs_;
    int64_t maxTimeUs_;
    int minPercentage_;
    int64_t lastTime_{0};
    float lastPercentage_{0};
};
