#include <freertos/FreeRTOS.h>
#include <freertos/timers.h>
#include <functional>
#include <utility>

#include "Mmc5603.hpp"

class TempleCloseSensor {
public:
    TempleCloseSensor() : magSensor_(0) {}

    void checkTemple();

    // Simple diagnostic test to check if the magnetometer is reporting the correct product ID
    auto checkProductId() -> bool;

    void setCallback(std::function<void(bool opened)> callback) { callback_ = std::move(callback); }

    // Callback fired after 15s of being closed
    void setLongCloseCallback(std::function<void()> callback) {
        longCloseCallback_ = std::move(callback);
    }

    auto getOpened() -> bool { return opened_; }

protected:
    Mmc5603 magSensor_;
    bool opened_ = false;
    std::function<void(bool opened)> callback_;
    std::function<void()> longCloseCallback_;

    TimerHandle_t longCloseTimer_;
    void resetLongCloseTimer();
};
