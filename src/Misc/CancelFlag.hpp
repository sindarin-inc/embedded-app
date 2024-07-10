#pragma once

// #include <esp_debug_helpers.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <functional>
#include <map>
#include <memory>
#include <string>

#include "sindarin-debug.h"

class CancelFlag {
public:
    CancelFlag() : mutex_(xSemaphoreCreateMutex()) {

        if (mutex_ == nullptr) {
            log_e("Failed to create cancel mutex");
        } else {
            log_d("Cancel mutex created successfully");
        }
    }

    ~CancelFlag() {
        if (mutex_ != nullptr) {
            vSemaphoreDelete(mutex_);
            log_d("Cancel mutex deleted successfully");
        }
    }

    CancelFlag(const CancelFlag &) = delete;
    auto operator=(const CancelFlag &) -> CancelFlag & = delete;

    void addObserver(void *key, std::function<void()> observer) {
        if (xSemaphoreTake(mutex_, portMAX_DELAY) == pdTRUE) {
            log_d("Adding observer with key: %p -> %p", key, &observer);
            observers_[key] = std::move(observer);
            xSemaphoreGive(mutex_);
        } else {
            log_e("Failed to take mutex in addObserver");
        }
    }

    void removeObserver(void *key) {
        if (xSemaphoreTake(mutex_, portMAX_DELAY) == pdTRUE) {
            observers_.erase(key);
            xSemaphoreGive(mutex_);
        } else {
            log_e("Failed to take mutex in removeObserver");
        }
    }

    static void cancel(const std::shared_ptr<CancelFlag> &flag) {
        if (flag == nullptr) {
            log_e("CancelFlag is null in cancel");
            return;
        }
        flag->cancelInternal();
    }

    static auto isCancelled(const std::shared_ptr<CancelFlag> &flag) -> bool {
        if (flag == nullptr) {
            // Print stack trace for esp
            // esp_backtrace_print(100);
            log_e("CancelFlag is null in isCancelled");
            return false;
        }
        return flag->isCancelledInternal();
    }

private:
    bool cancelled_ = false;
    std::unordered_map<void *, std::function<void()>> observers_;
    SemaphoreHandle_t mutex_;

    void cancelInternal() {
        if (xSemaphoreTake(mutex_, portMAX_DELAY) == pdTRUE) {
            log_d("Cancel flag set");
            cancelled_ = true;
            for (auto &observer : observers_) {
                observer.second();
            }
            xSemaphoreGive(mutex_);
        } else {
            log_e("Failed to take mutex in cancelInternal");
        }
    }

    [[nodiscard]] auto isCancelledInternal() const -> bool {
        if (xSemaphoreTake(mutex_, portMAX_DELAY) == pdTRUE) {
            bool cancelled = cancelled_;
            xSemaphoreGive(mutex_);
            return cancelled;
        }
        log_e("Failed to take mutex in isCancelledInternal");
        return false;
    }
};
