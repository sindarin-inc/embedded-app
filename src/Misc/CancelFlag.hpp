#pragma once

#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <functional>
#include <map>
#include <memory>
#include <string>

#include "sindarin-debug.h"

class CancelFlag {
public:
    CancelFlag() {
        mutex_ = xSemaphoreCreateMutex();
        if (mutex_ == nullptr) {
            log_e("Failed to create cancel mutex");
        }
    }

    ~CancelFlag() {
        if (mutex_ != nullptr) {
            vSemaphoreDelete(mutex_);
        }
    }

    CancelFlag(const CancelFlag &) = delete;
    CancelFlag &operator=(const CancelFlag &) = delete;

    void addObserver(void *key, std::function<void()> observer) {
        if (xSemaphoreTake(mutex_, portMAX_DELAY)) {
            log_d("Adding observer with key: %p -> %p", key, &observer);
            observers_[key] = std::move(observer);
            xSemaphoreGive(mutex_);
        }
    }

    void removeObserver(void *key) {
        if (xSemaphoreTake(mutex_, portMAX_DELAY)) {
            observers_.erase(key);
            xSemaphoreGive(mutex_);
        }
    }

    static void cancel(const std::shared_ptr<CancelFlag> &flag) {
        if (flag == nullptr) {
            return;
        }
        flag->cancelInternal();
    }

    static bool isCancelled(const std::shared_ptr<CancelFlag> &flag) {
        if (flag == nullptr) {
            return false;
        }
        return flag->isCancelledInternal();
    }

private:
    bool cancelled_ = false;
    std::map<void *, std::function<void()>> observers_;
    SemaphoreHandle_t mutex_;

    void cancelInternal() {
        if (xSemaphoreTake(mutex_, portMAX_DELAY)) {
            cancelled_ = true;
            for (auto &observer : observers_) {
                observer.second();
            }
            xSemaphoreGive(mutex_);
        }
    }

    bool isCancelledInternal() const {
        if (xSemaphoreTake(mutex_, portMAX_DELAY)) {
            bool cancelled = cancelled_;
            xSemaphoreGive(mutex_);
            return cancelled;
        }
        return false;
    }
};
