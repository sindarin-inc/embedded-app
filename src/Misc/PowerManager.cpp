#include "PowerManager.hpp"

#include <esp_sleep.h>
#include <esp_system.h>

#include "Platform.hpp"

extern PowerManager powerManager;

auto PowerManager::getInstance() -> PowerManager & { return powerManager; }

void PowerManager::powerOff() {
    log_i("Starting power off sequence");
    // If we have syncing to do for newspaper drop, deep sleep only (waking up regularly to check)
    // Otherwise, just turn off the ESP32
    log_i("Prefs for morning and evening are %d %d (0 means not set)",
          prefs_.morningNewspaperTime.get().value_or(0),
          prefs_.eveningNewspaperTime.get().value_or(0));

    if ((prefs_.morningNewspaperTime.get().has_value()) ||
        (prefs_.eveningNewspaperTime.get().has_value())) {
        log_i("Setting wakeup timer for deep sleep to 5s");
        esp_sleep_enable_timer_wakeup(5000000);
        log_i("Going to sleep -- good night!");
        // If we don't tell the system to keep the power on, it'll turn off the ESP32 entirely
        // when the temple is closed. This is a problem because we need to be able to wake up.
        ForcePowerOn();
        esp_deep_sleep_start();
        // In glass, this will never execute. On simulator it will.
        return;
    }

    AllowPowerOff();
}