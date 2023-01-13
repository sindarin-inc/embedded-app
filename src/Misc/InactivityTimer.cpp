#include "InactivityTimer.hpp"

#include "sindarin-debug.h"

InactivityTimer *inactivityTimerInstance = nullptr;

InactivityTimer::InactivityTimer() : callback_(nullptr) {
    if (inactivityTimerInstance == nullptr) {
        inactivityTimerInstance = this;
    } else {
        log_e("InactivityTimer::InactivityTimer() called more than once");
    }
}

auto InactivityTimer::getInstance() -> InactivityTimer * {
    if (inactivityTimerInstance == nullptr) {
        log_e("InactivityTimer::getInstance() called before InactivityTimer created");
    }
    return inactivityTimerInstance;
}
