#pragma once

#include <freertos/FreeRTOS.h>
#include <freertos/timers.h>
#include <functional>
#include <utility>

class InactivityTimer {
public:
    InactivityTimer() : callback_(nullptr) {}

    ~InactivityTimer() {
        if (timer_ != nullptr) {
            xTimerDelete(timer_, portMAX_DELAY);
            timer_ = nullptr;
        }
    }

    void start(std::function<void()> callback, int inactivityTimeoutMs = 10 * 60 * 1000) {
        callback_ = std::move(callback);

        // Create the timer and start it.
        timer_ = xTimerCreate("InactivityTimer", pdMS_TO_TICKS(inactivityTimeoutMs), pdFALSE,
                              reinterpret_cast<void *>(this), &InactivityTimer::timerCallback);
        xTimerStart(timer_, portMAX_DELAY);
    }

    void setTimeoutMs(int timeoutMs) {
        if (timer_ == nullptr) {
            return;
        }
        xTimerStop(timer_, 0);
        xTimerChangePeriod(timer_, pdMS_TO_TICKS(timeoutMs), portMAX_DELAY);
    }

    void feed() {
        if (timer_ == nullptr) {
            return;
        }
        xTimerReset(timer_, portMAX_DELAY);
    }

    void pause() {
        if (timer_ == nullptr) {
            return;
        }
        xTimerStop(timer_, portMAX_DELAY);
    }

    void resume() {
        if (timer_ == nullptr) {
            return;
        }
        xTimerStart(timer_, portMAX_DELAY);
    }

private:
    static void timerCallback(TimerHandle_t timer) {
        // Get the instance of the InactivityTimer from the timer handle.
        auto *instance = reinterpret_cast<InactivityTimer *>(pvTimerGetTimerID(timer));
        if (instance->callback_) {
            instance->callback_();
        }
    }

    TimerHandle_t timer_{nullptr};
    std::function<void()> callback_;
};