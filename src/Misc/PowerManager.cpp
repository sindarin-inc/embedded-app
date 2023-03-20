#include "PowerManager.hpp"

#if !SIMULATOR
#include "FunctionalInterrupt.h"
#endif

#include <esp_sleep.h>
#include <esp_system.h>

#include "Misc/Dispatch.hpp"
#include "Platform.hpp"

extern PowerManager powerManager;

auto PowerManager::getInstance() -> PowerManager & { return powerManager; }

auto PowerManager::begin() -> esp_err_t {
#if !SIMULATOR
    inactivityTimer_.start([this]() {
        log_d("Inactive! Allow power off.");
        prefs_.sleepReason.set(SLEEP_REASON_INACTIVITY);
        powerOff();
    });
#endif

    esp_err_t err = ESP_OK;
#if !DISABLE_POWER_MANAGEMENT
    pmConfig_.max_freq_mhz = CPU_FREQ_MHZ_HIGH;
    pmConfig_.min_freq_mhz = CPU_FREQ_MHZ_LOW;
    pmConfig_.light_sleep_enable = !USBConnected();
    err = esp_pm_configure(&pmConfig_);

#ifdef USB_CONNECTED_PIN
    attachInterrupt(
        digitalPinToInterrupt(USB_CONNECTED_PIN),
        []() {
#if !DISABLE_POWER_MANAGEMENT
            DispatchGetCore1BackgroundQueue()->dispatch(
                []() { PowerManager::getInstance().updateState(); });
#endif
        },
        CHANGE);
#endif
#endif
    return err;
}

void PowerManager::powerOff() {
    log_i("Starting power off sequence");
    // If we have syncing to do for newspaper drop, deep sleep only (waking up regularly to check)
    // Otherwise, just turn off the ESP32
    log_i("Has value for morning and evening are %d %d (0 means not set)",
          prefs_.morningNewspaperTime.get().has_value(),
          prefs_.eveningNewspaperTime.get().has_value());

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

void PowerManager::updateState() {
#if !DISABLE_POWER_MANAGEMENT
    if (USBConnected() && pmConfig_.light_sleep_enable == true) {
        // USB is connected so disable light sleep
        pmConfig_.light_sleep_enable = false;
        auto err = esp_pm_configure(&pmConfig_);
        if (err != ESP_OK) {
            log_e("Failed to disable light sleep with err %s", esp_err_to_name(err));
        } else {
            log_d("Disabled light sleep");
        }
        inactivityTimer_.pause();
    } else if (!USBConnected() && pmConfig_.light_sleep_enable == false) {
        // USB is disconnected so enable light sleep
        pmConfig_.light_sleep_enable = true;
        auto err = esp_pm_configure(&pmConfig_);
        if (err != ESP_OK) {
            log_e("Failed to enable light sleep with err %s", esp_err_to_name(err));
        } else {
            log_d("Enabled light sleep");
        }
        inactivityTimer_.resume();
    }
#endif
}
