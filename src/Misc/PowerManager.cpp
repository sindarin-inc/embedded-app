#include "PowerManager.hpp"

#include <cstddef>
#include <driver/gpio.h>
#include <driver/rtc_io.h>
#include <esp_idf_version.h>
#include <esp_sleep.h>
#include <esp_system.h>
#include <hal/gpio_types.h>
#include <rom/rtc.h>
#include <ulp_riscv.h>
#include <ulp_riscv_i2c.h>

#include "Misc/Dispatch.hpp"
#include "Platform.hpp"

#if !SIMULATOR
#include "FunctionalInterrupt.h"
#endif

// These are also in build/esp-idf/src/ulp_src/ulp_src.h but including that file throws a lot of
// unused vairable warnings
extern uint32_t ulp_waitForClose; // NOLINT(readability-identifier-naming)

RTC_SLOW_ATTR bool ulpLoaded = false;

extern const uint8_t ULP_MAIN_BIN_START[] asm("_binary_ulp_src_bin_start");
extern const uint8_t ULP_MAIN_BIN_END[] asm("_binary_ulp_src_bin_end");

#define ULP_RUN_INTERVAL_MS 1000

extern PowerManager powerManager;

auto PowerManager::getInstance() -> PowerManager & { return powerManager; }

void PowerManager::wakeupCheck(SolClock &clock) {
    // If you want to see this using `idf.py monitor --no-reset` you'll need to delay a bit for the
    // USB to connect.
    if (USBConnected()) {
        Delay(1000);
    }

    esp_reset_reason_t resetReason = esp_reset_reason();
    switch (resetReason) {
    case ESP_RST_UNKNOWN: {
        auto rtcResetReason = rtc_get_reset_reason(0);
        switch (rtcResetReason) {
        case USB_UART_CHIP_RESET:
            log_w("Reboot triggered by USB UART");
            break;
        default:
            log_w("Reboot triggered by unknown reason (rtc reason = %d)", rtcResetReason);
            break;
        };
        break;
    }
    case ESP_RST_POWERON:
        log_i("Booting from power on");
        break;
    case ESP_RST_SW:
        log_i("Booting from software reset");
        break;
    case ESP_RST_PANIC:
    case ESP_RST_INT_WDT:
    case ESP_RST_TASK_WDT:
        // TODO: boot loop detector from here?
        // TODO: if we crashed while in a background sync either try again or go back to sleep. But
        // we need a way to communicate that.
        log_i("Booting from unexpected software failure");
        break;
    case ESP_RST_DEEPSLEEP: {
        log_i("Booting from deep sleep");
        esp_sleep_wakeup_cause_t sleepWakeupCause = esp_sleep_get_wakeup_cause();
        switch (sleepWakeupCause) {
        case ESP_SLEEP_WAKEUP_TIMER:
            // We're waking up from a deep sleep timer. We probably wan't to content sync.
            log_i("Waking up from deep sleep timer");
            state_ = PowerState::Background;
            break;
        case ESP_SLEEP_WAKEUP_ULP:
            // We're waking up from a ULP trigger. We probably want to go to active.
            log_i("Waking up from deep sleep ulp trigger (mag sensor)");
            break;
        case ESP_SLEEP_WAKEUP_EXT0: {
            // We're waking up because USB was connected (so we can set the LED)
            log_i("Waking up from deep sleep ext0 (USB connected)");
            // Check if the USB LED should be on
            checkUsb();
            auto percentage = battery.getBatteryPercentage();
            if (USBConnected()) {
                log_i("USB connected, battery level: %d%%", int(percentage));
            }
            // Wait just a little moment so we can flash software if we get stuck
            Delay(1000);
            // Back to sleep
            ulpDeepSleep(SleepReason::UpdateUSBState);
            break;
        }
        case ESP_SLEEP_WAKEUP_COCPU_TRAP_TRIG:
            log_e("ULP crashed");
            // Back to sleep
            ulpDeepSleep(SleepReason::UlpCrashed);
            break;
        default:
            log_w("Unexpected deep sleep wakeup cause: %d", sleepWakeupCause);
            break;
        }
        break;
    }
    case ESP_RST_BROWNOUT:
        // TODO: Consider hard power off after brownout?
        log_i("Booting from brownout");
        break;
    case ESP_RST_WDT:
    case ESP_RST_SDIO:
    case ESP_RST_EXT:
    default:
        log_w("Unexpected reset reason: %d", resetReason);
        break;
    }

    checkUsb();

    // At this point state_ is either Background or Booting. If it's Booting, we likely want to
    // proceed to Active
    if (state_ == PowerState::Booting) {
        // If the temple is closed but we're active booting, probably something wrong happened (like
        // a crash during content sync). Just go back to sleep.
        bool open = true;
#if USE_MMC5603
        open = templeCloseSensor_.isTempleOpen();
#endif
        if (!open) {
            log_w("Temple is closed at boot, going back to sleep");
            ulpDeepSleep(SleepReason::TempleClosedAtBoot);
        }

        state_ = PowerState::Active;
        log_i("Beginning active boot");
    } else if (state_ == PowerState::Background) {
        auto now = SolClock::getInstance().getEpochTime();
        if (PreferencesStore::nextSyncTime && now >= PreferencesStore::nextSyncTime) {
            log_i("Time for background content sync (%" PRIu64 " (now) >= %" PRIu64 ")",
                  int64_t(now), PreferencesStore::nextSyncTime);
        } else {
            log_i("Next sync time is %" PRIu64 ". Back to sleep.", PreferencesStore::nextSyncTime);
            ulpDeepSleep(SleepReason::WaitingForSync);
        }
    } else {
        log_w("Unexpectedly resuming boot");
    }
}

auto PowerManager::begin() -> esp_err_t {
#if !SIMULATOR
    inactivityTimer_.start([this]() {
        log_i("Inactive! Close and open temple to wake");
        ulpDeepSleep(SleepReason::Inactivity);
    });
#endif

    esp_err_t err = ESP_OK;
#if !DISABLE_POWER_MANAGEMENT
    pmConfig_.max_freq_mhz =
        state_ == PowerState::Background ? CPU_FREQ_MHZ_BACKGROUND : CPU_FREQ_MHZ_HIGH;
    pmConfig_.min_freq_mhz = CPU_FREQ_MHZ_LOW;
    pmConfig_.light_sleep_enable = !USBConnected();
    err = esp_pm_configure(&pmConfig_);

#ifdef USB_CONNECTED_PIN
    attachInterrupt(
        digitalPinToInterrupt(USB_CONNECTED_PIN),
        [this]() {
#if !DISABLE_POWER_MANAGEMENT
        DispatchGetCore1BackgroundQueue()->dispatch([this]() {
            if (USBConnected() && pmConfig_.light_sleep_enable == true) {
                // USB is connected so disable light sleep
                pmConfig_.light_sleep_enable = false;
                auto err = esp_pm_configure(&pmConfig_);
                if (err != ESP_OK) {
                    log_e("Failed to disable light sleep with err %s", esp_err_to_name(err));
                } else {
                    log_d("Disabled light sleep");
                }
            } else if (!USBConnected() && pmConfig_.light_sleep_enable == false) {
                // USB is disconnected so enable light sleep
                pmConfig_.light_sleep_enable = true;
                auto err = esp_pm_configure(&pmConfig_);
                if (err != ESP_OK) {
                    log_e("Failed to enable light sleep with err %s", esp_err_to_name(err));
                } else {
                    log_d("Enabled light sleep");
                }
            }
        });
#endif
        },
        CHANGE);
#endif
#endif

    templeCloseSensor_.setCallback([this](bool open) {
        switch (state_) {
        case PowerState::Active:
            if (!open) {
                lightingController_.disableLighting(200);
                // Delay to see the fade.
                // TODO: Use a callback instead
                Delay(200);
                ulpDeepSleep(SleepReason::TempleClosed);
            }
            break;
        case PowerState::Background:
            if (open) {
                log_i("Temples opened while in Background! Going to Active");
                // Temples were opened while in the background! Wake up with a reboot.
                reboot();
            }
            break;
        default:
            break;
        };
    });

    return err;
}

void PowerManager::powerOff() {
    log_i("Starting power off sequence");
    AllowPowerOff();
    esp_deep_sleep_start();
}

void PowerManager::loop() {
    battery.loop();
    checkUsb();
#if USE_MMC5603
    templeCloseSensor_.checkTemple();
#endif
}

void PowerManager::checkUsb() {
    if (USBConnected()) {
        // TODO: Should we take multiple measurements?
        battery.loop();
        auto percentage = battery.getBatteryPercentage();
        gpio_hold_dis((gpio_num_t)GREEN_LED);
        gpio_hold_dis((gpio_num_t)RED_LED);
        if (percentage > 95) {
            // If the battery is fully charged, turn the GREEN LED on.
            DigitalWrite(GREEN_LED, LED_ON);
            DigitalWrite(RED_LED, LED_OFF);
        } else {
            DigitalWrite(GREEN_LED, LED_OFF);
            DigitalWrite(RED_LED, LED_ON);
        }
        gpio_hold_en((gpio_num_t)GREEN_LED);
        gpio_hold_en((gpio_num_t)RED_LED);
        gpio_deep_sleep_hold_en();
    } else {
        DigitalWrite(GREEN_LED, LED_OFF);
        DigitalWrite(RED_LED, LED_OFF);
        gpio_hold_dis((gpio_num_t)GREEN_LED);
        gpio_hold_dis((gpio_num_t)RED_LED);
    }
}

void PowerManager::reboot() { esp_restart(); }

void PowerManager::ulpDeepSleep(SleepReason reason) {
#if DISABLE_DEEP_SLEEP
    log_w("Deep sleep disabled. Not sleeping.");
    return;
#endif

    battery.saveLastBatteryReport();

    // Really this probably only needs to be the screens and the filesystem that we shut down
    // gracefully. Everything else should be fine to just let die. We should consider moving all
    // filesystem operations into their own queue and just shutting down that queue and the UI
    // queue.
    bool isExecutingInQueue = false;
    auto core0BgQueue = DispatchGetCore0BackgroundQueue(false);
    if (core0BgQueue != nullptr) {
        isExecutingInQueue = isExecutingInQueue || core0BgQueue->isInQueue();
        core0BgQueue->beginShutdown();
    }
    auto core1BgQueue = DispatchGetCore1BackgroundQueue(false);
    if (core1BgQueue != nullptr) {
        isExecutingInQueue = isExecutingInQueue || core1BgQueue->isInQueue();
        core1BgQueue->beginShutdown();
    }
    auto uiQueue = DispatchGetUiQueue(false);
    if (uiQueue != nullptr) {
        isExecutingInQueue = isExecutingInQueue || uiQueue->isInQueue();
        uiQueue->beginShutdown();
    }

    // We do the final shutdown sequence in a new task, in case the shutdown was running in one of
    // the queues that needed to be shut down
    auto taskName = pcTaskGetName(nullptr);
    log_d("ulpDeepSleep executing from task %s", taskName);
    auto reasonPtr = new SleepReason(reason);
    BaseType_t status = xTaskCreatePinnedToCore(
        [](void *reasonPtr) {
        SleepReason reason = *reinterpret_cast<SleepReason *>(reasonPtr);
        PowerManager::getInstance().waitForQueuesThenDeepSleep(reason);
        },
        "DeepSleep", 4096, (void *)reasonPtr, 1, nullptr, 1);
    if (status != pdPASS) {
        log_e("Failed to create DeepSleep task");
        powerOff();
        return;
    }

    if (!isExecutingInQueue) {
        // If we're not executing in a queue just shut down this task
        vTaskDelete(nullptr);
        // Shouldn't ever get here
        log_e("ulpDeepSleep task deleted itself but still running!");
    }
}

auto ToString(SleepReason reason) -> std::string {
    switch (reason) {
    case SleepReason::Unspecified:
        return "Unspecified";
    case SleepReason::Inactivity:
        return "Inactivity";
    case SleepReason::TempleClosed:
        return "TempleClosed";
    case SleepReason::TempleClosedAtBoot:
        return "TempleClosedAtBoot";
    case SleepReason::SyncDone:
        return "SyncDone";
    case SleepReason::UlpCrashed:
        return "UlpCrashed";
    case SleepReason::UpdateUSBState:
        return "UpdateUSBState";
    case SleepReason::WaitingForSync:
        return "WaitingForSync";
    default:
        return "Unknown";
    }
}

void PowerManager::waitForQueuesThenDeepSleep(SleepReason reason) {
    log_d("Beginning shutdown sequence with reason %s", ToString(reason).c_str());

    log_d("Waiting for dispatch queues to shutdown");
    auto core0BgQueue = DispatchGetCore0BackgroundQueue(false);
    if (core0BgQueue) {
        core0BgQueue->waitForShutdown(5000);
    }
    auto core1BgQueue = DispatchGetCore1BackgroundQueue(false);
    if (core1BgQueue) {
        core1BgQueue->waitForShutdown(5000);
    }
    auto uiQueue = DispatchGetUiQueue(false);
    if (uiQueue) {
        uiQueue->waitForShutdown(5000);
    }
    log_d("Dispatch queues shutdown");

    bool syncOnClose = (reason == SleepReason::TempleClosed || reason == SleepReason::Inactivity) &&
                       !prefs_.noDeliverOnClose.get().value_or(false);
    if (syncOnClose) {
        // On temples close or inactivity we want to set a sync-on-close timer 30s after sleep
        // starts. This is mostly to save placemarks after reading
        if (reason == SleepReason::TempleClosed) {
            PreferencesStore::syncReason = SyncReason::OnClose;
            PreferencesStore::nextSyncTime = SolClock::getInstance().getEpochTime() + 30;
            log_d("Sync-on-close scheduled for %" PRIu64, PreferencesStore::nextSyncTime);
        } else if (reason == SleepReason::Inactivity) {
            PreferencesStore::syncReason = SyncReason::OnInactivity;
            PreferencesStore::nextSyncTime = SolClock::getInstance().getEpochTime() + 30;
            log_d("Sync-after-inactivity scheduled for %" PRIu64, PreferencesStore::nextSyncTime);
        }
    }

    // Make sure the ambient light sensor is off so it's not using power while in deep sleep
    lightingController_.disableLighting(0);

    prefs_.end();

#if SIMULATOR
    log_i("ulpDeepSleep");
#else

    esp_err_t err;

    if (BLEDevice::getInitialized()) {
        BLEDevice::deinit();
    }

    err = DeinitializePlatform();
    if (err != ESP_OK) {
        log_e("Failed to deinit platform with err %s", esp_err_to_name(err));
    }

    err = HardwareDeinit();
    if (err != ESP_OK) {
        log_e("Failed to deinit hardware with err %s", esp_err_to_name(err));
    }

    err = rtc_gpio_init((gpio_num_t)I2C_MASTER_SDA_IO);
    if (err != ESP_OK) {
        log_e("Failed to init SDA with err %s", esp_err_to_name(err));
    }
    err = rtc_gpio_init((gpio_num_t)I2C_MASTER_SCL_IO);
    if (err != ESP_OK) {
        log_e("Failed to init SCL with err %s", esp_err_to_name(err));
    }

    ulp_riscv_i2c_cfg_t i2cCfg;
    i2cCfg.i2c_pin_cfg.sda_io_num = I2C_MASTER_SDA_IO;
    i2cCfg.i2c_pin_cfg.scl_io_num = I2C_MASTER_SCL_IO;
    i2cCfg.i2c_pin_cfg.sda_pullup_en = true;
    i2cCfg.i2c_pin_cfg.scl_pullup_en = true;
    i2cCfg.i2c_timing_cfg.scl_low_period = 5;
    i2cCfg.i2c_timing_cfg.scl_high_period = 5;
    i2cCfg.i2c_timing_cfg.sda_duty_period = 2;
    i2cCfg.i2c_timing_cfg.scl_start_period = 3;
    i2cCfg.i2c_timing_cfg.scl_stop_period = 6;
    i2cCfg.i2c_timing_cfg.i2c_trans_timeout = 20;
    err = ulp_riscv_i2c_master_init(&i2cCfg);
    if (err != ESP_OK) {
        log_e("Failed to init I2C with err %s", esp_err_to_name(err));
        // If the magnetometer is not working we can never wake up. Power off instead.
        powerOff();
        return;
    }

    // `ulp_riscv_load_binary` seems to reset all the ulp variables including `ulp_waitForClose`
    // which we only want to unset if the temples are closed.
    if (!ulpLoaded) {
        err = ulp_riscv_load_binary(ULP_MAIN_BIN_START, (ULP_MAIN_BIN_END - ULP_MAIN_BIN_START));
        if (err != ESP_OK) {
            log_e("load binary err %d", err);
            return;
        }
        ulpLoaded = true;
    }

    // If we're sleeping and the temples are already open, tell the ULP to wait for them to be
    // closed first before waking up via the magnetometer
    if (reason == SleepReason::Inactivity) {
        log_d("Inactivity while temple open, waiting for temple close before waking up again");
        ulp_waitForClose = 1;
    }

    ulp_set_wakeup_period(0, ULP_RUN_INTERVAL_MS * 1000);

    // Start the ULP program
    err = ulp_riscv_run();
    if (err != ESP_OK) {
        log_e("ulp_riscv_run error %d", err);
        powerOff();
        return;
    }

    err = esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_PERIPH, ESP_PD_OPTION_ON);
    if (err != ESP_OK) {
        log_e("error enabling ESP_PD_DOMAIN_RTC_PERIPH power domain %d", err);
        powerOff();
        return;
    }

    err = esp_sleep_enable_ulp_wakeup();
    if (err != ESP_OK) {
        log_e("error enabling ulp wakeup from deep sleep %d", err);
        powerOff();
        return;
    }

    // Enable a wakeup on the USB_CONNECTED_PIN
    if (USBConnected()) {
        err = esp_sleep_enable_ext0_wakeup((gpio_num_t)USB_CONNECTED_PIN, 0);
        if (err != ESP_OK) {
            log_e("error enabling USB_CONNECTED_PIN low wakeup from deep sleep %d", err);
            powerOff();
            return;
        }
    } else {
        err = esp_sleep_enable_ext0_wakeup((gpio_num_t)USB_CONNECTED_PIN, 1);
        if (err != ESP_OK) {
            log_e("error enabling USB_CONNECTED_PIN high wakeup from deep sleep %d", err);
            powerOff();
            return;
        }
    }

    // By default sync every 24 hours
    auto sleepTime = uint64_t(24) * 60 * 60 * 1000000;

    // When USB is connected, we want to wake up minutely and check the battery level (to know
    // when to turn the LED to green), otherwise we cant wait until the next content sync.
    if (USBConnected()) {
        sleepTime = 60 * uint64_t(1000000);
    }

    // Otherwise we want to wait until we want to run our next content sync if that's sooner
    // than what we've already set
    sleepTime = std::min(sleepTime,
                         (PreferencesStore::nextSyncTime - SolClock::getInstance().getEpochTime()) *
                             uint64_t(1000000));

    err = esp_sleep_enable_timer_wakeup(sleepTime);
    if (err != ESP_OK) {
        log_e("error enabling timer wakeup for deep sleep %d", err);
    } else {
        log_d("Sleeping for %" PRIu64 " seconds", sleepTime / 1000000);
    }

    log_i("Entering deep sleep, wake up with magnetometer");
    esp_deep_sleep_start();
#endif
}
