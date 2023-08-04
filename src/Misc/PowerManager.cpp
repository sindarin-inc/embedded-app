#include "PowerManager.hpp"

#include <cstddef>
#include <driver/gpio.h>
#include <driver/rtc_io.h>
#include <esp_idf_version.h>
#include <hal/gpio_types.h>
#include <ulp_riscv.h>
#include <ulp_riscv_i2c.h>

#include "Misc/Dispatch.hpp"
#include "Platform.hpp"
#include "Sensors/TempleAngle.hpp"

#if !SIMULATOR
#include "FunctionalInterrupt.h"
#else
uint32_t ulp_waitForClose;
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

    // From these three variables we can figure out exactly why we're booting
    bootInfo_.resetReason = esp_reset_reason();
    bootInfo_.rtcResetReason = rtc_get_reset_reason(0);
    bootInfo_.sleepWakeupCause = esp_sleep_get_wakeup_cause();

    switch (bootInfo_.resetReason) {
    case ESP_RST_UNKNOWN: {
        switch (bootInfo_.rtcResetReason) {
        case USB_UART_CHIP_RESET:
            bootInfo_.reason = BootInfo::Reason::UartChipReset;
            break;
        default:
            bootInfo_.reason = BootInfo::Reason::Unknown;
            break;
        };
        break;
    }
    case ESP_RST_POWERON:
        bootInfo_.reason = BootInfo::Reason::PowerOn;
        break;
    case ESP_RST_SW:
        bootInfo_.reason = BootInfo::Reason::Sw;
        break;
    case ESP_RST_PANIC:
        // TODO: boot loop detector from here?
        // TODO: If we crashed while in a background sync either try again or go back to sleep. But
        // we need a way to communicate that.
        bootInfo_.reason = BootInfo::Reason::Panic;
        break;
    case ESP_RST_INT_WDT:
        bootInfo_.reason = BootInfo::Reason::IntWdt;
        break;
    case ESP_RST_TASK_WDT:
        bootInfo_.reason = BootInfo::Reason::TaskWdt;
        break;
    case ESP_RST_DEEPSLEEP: {
        log_i("Booting from deep sleep");
        switch (bootInfo_.sleepWakeupCause) {
        case ESP_SLEEP_WAKEUP_TIMER:
            bootInfo_.reason = BootInfo::Reason::Timer;
            // We're waking up from a deep sleep timer. We probably wan't to content sync.
            state_ = PowerState::Background;
            break;
        case ESP_SLEEP_WAKEUP_ULP:
            bootInfo_.reason = BootInfo::Reason::UlpMag;
            // We're waking up from a ULP trigger. We probably want to go to active.
            break;
        case ESP_SLEEP_WAKEUP_EXT0: {
            bootInfo_.reason = BootInfo::Reason::UsbPlug;
            // We're waking up because USB was connected (so we can set the LED)
            // Check if the USB LED should be on
            checkUsb();
            auto percentage = battery.getBatteryPercentage();
            if (USBConnected()) {
                log_i("USB connected, battery level: %d%%", int(percentage));
                // Wait just a little moment so we can flash software if we get stuck
                Delay(1000);
            } else if (ulp_waitForClose) {
                ulp_waitForClose = 0;
                // If the temples were never closed and usb just disconnected, let's power up
                log_i("Temple is still open on USB disconnect, turning on");
                break;
            } else {
                // USB disconnected with temples closed, go back to sleep
                log_i("USB disconnected, battery level: %d%%", int(percentage));
            }
            // Back to sleep
            ulpDeepSleep(SleepReason::UpdateUSBState);
            break;
        }
        case ESP_SLEEP_WAKEUP_COCPU_TRAP_TRIG:
            bootInfo_.reason = BootInfo::Reason::CoCpuTrap;
            log_e("ULP crashed");
            // Back to sleep
            ulpDeepSleep(SleepReason::UlpCrashed);
            break;
        default:
            bootInfo_.reason = BootInfo::Reason::Unknown;
            break;
        }
        break;
    }
    case ESP_RST_BROWNOUT:
        bootInfo_.reason = BootInfo::Reason::Brownout;
        // TODO: Consider hard power off after brownout?
        break;
    case ESP_RST_WDT:
        bootInfo_.reason = BootInfo::Reason::Wdt;
        break;
    case ESP_RST_SDIO:
    case ESP_RST_EXT:
        bootInfo_.reason = BootInfo::Reason::Unknown;
        break;
    }

    if (bootInfo_.reason == BootInfo::Reason::Unknown) {
        log_e("Unknown boot reason (esp_reset_reason=%d, rtc_get_reset_reason=%d, "
              "esp_sleep_get_wakeup_cause=%d) ",
              bootInfo_.resetReason, bootInfo_.rtcResetReason, bootInfo_.sleepWakeupCause);
    } else {
        log_i("Boot reason: %s", bootInfo_.humanStr().c_str());
    }

    checkUsb();

    // At this point state_ is either Background or Booting. If it's Booting, we likely want to
    // proceed to Active
    esp_err_t err = isTempleOpen(templeOpen_);
    if (state_ == PowerState::Booting) {
        // If the temple is closed but we're active booting, probably something wrong happened (like
        // a crash during content sync). Just go back to sleep.
        if (err != ESP_OK) {
            log_e("Error checking temple state: %d, continuing boot", err);
        } else if (!templeOpen_) {
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
    // If USB is connected, disable operation unless we've disabled this behavior
    if (USBConnected() && state_ == PowerState::Active &&
        !prefs_.allowUsageWhileCharging.get().value_or(false)) {
        log_i("USB connected, switching mode to ActiveUSBCharging");
        state_ = PowerState::ActiveUSBCharging;
    }
    // Disable inactivity timer in emc testing mode
    if (!prefs_.emcMode.get().value_or(false)) {
        int inactivityTimeoutMs =
            state_ == PowerState::ActiveUSBCharging ? 15 * 1000 : 10 * 60 * 1000;
        log_d("Setting inactivity timeout to %d ms", inactivityTimeoutMs);
        inactivityTimer_.start(
            [this]() {
            log_i("Inactive! Close and open temple to wake");
            ulpDeepSleep(SleepReason::Inactivity);
            },
            inactivityTimeoutMs);
    }
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
            // If USB is connected, disable operation unless we've disabled this behavior
            if (USBConnected() && !prefs_.allowUsageWhileCharging.get().value_or(false)) {
                log_i("USB connected, switching to ActiveUSBCharging");
                state_ = PowerState::ActiveUSBCharging;
                inactivityTimer_.setTimeoutMs(10 * 1000);
                DispatchGetUiQueue()->dispatch([this]() {
                    if (stateChangeCallback_ != nullptr) {
                        stateChangeCallback_(state_);
                    }
                });
            } else if (!USBConnected() && state_ == PowerState::ActiveUSBCharging) {
                log_i("USB disconnected, rebooting to Active");
                reboot();
            } else if (USBConnected() && pmConfig_.light_sleep_enable == true) {
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
    bool templeOpen;
    auto err = isTempleOpen(templeOpen);
    if (err != ESP_OK) {
        log_e("Error checking temple state %d, ignoring.", err);
    } else if (templeOpen_ != templeOpen) {
        log_i("Temple is %s", templeOpen ? "open" : "closed");
        switch (state_) {
        case PowerState::Active:
        case PowerState::ActiveUSBCharging:
        case PowerState::ActiveOverTemp:
            if (!templeOpen) {
                ulpDeepSleep(SleepReason::TempleClosed);
            }
            break;
        case PowerState::Background:
            if (templeOpen) {
                log_i("Temples opened while in Background! Going to Active");
                // Temples were opened while in the background! Wake up with a reboot.
                reboot();
            }
            break;
        // Explicitly don't do anything in the ActiveForcedOn state when temples change.
        case PowerState::ActiveForcedOn:
        default:
            break;
        };
        templeOpen_ = templeOpen;
    }

    float temperature;
    err = magSensor_.readTemperature(temperature);
    if (err != ESP_OK) {
        log_e("Error reading temperature %d, ignoring.", err);
    } else if (state_ != PowerState::ActiveOverTemp &&
               temperature > MAX_OPERATIONAL_TEMPERATURE_C) {
        log_e("Temperature is too high! Going to sleep.");
        state_ = PowerState::ActiveOverTemp;
        inactivityTimer_.setTimeoutMs(10 * 1000);
        DispatchGetUiQueue()->dispatch([this]() {
            if (stateChangeCallback_ != nullptr) {
                stateChangeCallback_(state_);
            }
        });
    }

#endif
}

auto PowerManager::isTempleOpen(bool &open) -> esp_err_t {
    Xyz8bit magReading;
#if USE_MMC5603
    auto err = magSensor_.readXZ8Bit(magReading);
    if (err != ESP_OK) {
        log_e("Failed to read magnetometer");
        return err;
    }
    open = IsTempleOpen(magReading.x, magReading.z);
#else
    open = true;
#endif
    return ESP_OK;
}

void PowerManager::checkUsb() {
    if (USBConnected()) {
        // TODO: Should we take multiple measurements?
        battery.loop();
        auto percentage = battery.getBatteryPercentage();
        gpio_hold_dis((gpio_num_t)GREEN_LED);
        gpio_hold_dis((gpio_num_t)RED_LED);

        if (percentage > 95) {
            const uint32_t delayPeriod = 30 * 60; // 30 min delay upon reaching 95% battery level
            time_t currentTime = SolClock::getInstance().getEpochTime();

            if (PreferencesStore::ledChargeTime == 0) {
                // If the battery is fully charged, start the timer before turning on the LED
                PreferencesStore::ledChargeTime = currentTime + delayPeriod;
            } else if (currentTime >= PreferencesStore::ledChargeTime + delayPeriod) {
                // If the battery is fully charged and a half hour has passed, turn on the LED
                DigitalWrite(GREEN_LED, LED_ON);
                DigitalWrite(RED_LED, LED_OFF);
                gpio_hold_en((gpio_num_t)GREEN_LED);
                gpio_hold_en((gpio_num_t)RED_LED);
                gpio_deep_sleep_hold_en();
            }
        } else {
            PreferencesStore::ledChargeTime = 0; // reset the timer as battery level is below 95%
            DigitalWrite(GREEN_LED, LED_OFF);
            DigitalWrite(RED_LED, LED_ON);
            gpio_hold_en((gpio_num_t)GREEN_LED);
            gpio_hold_en((gpio_num_t)RED_LED);
            gpio_deep_sleep_hold_en();
        }
    } else {
        PreferencesStore::ledChargeTime = 0; // reset the timer as the USB is disconnected
        DigitalWrite(GREEN_LED, LED_OFF);
        DigitalWrite(RED_LED, LED_OFF);
        gpio_hold_dis((gpio_num_t)GREEN_LED);
        gpio_hold_dis((gpio_num_t)RED_LED);
    }
}

void PowerManager::releaseLED() {
    DigitalWrite(GREEN_LED, LED_OFF);
    DigitalWrite(RED_LED, LED_OFF);
    gpio_hold_dis((gpio_num_t)GREEN_LED);
    gpio_hold_dis((gpio_num_t)RED_LED);
}

void PowerManager::beginShutdown() {
    auto taskName = pcTaskGetName(nullptr);
    log_d("beginShutdown from task %s", taskName);

    battery.saveLastBatteryReport();

    if (beginShutdownCallback_ != nullptr) {
        beginShutdownCallback_();
    }

    if (state_ == PowerState::Active || state_ == PowerState::ActiveOverTemp ||
        state_ == PowerState::ActiveUSBCharging || state_ == PowerState::ActiveForcedOn) {
        lightingController_.disableLighting(200);
        // Delay to see the fade.
        // TODO: Use a callback instead
        Delay(200);
        auto clearScreen = [this]() {
            display_.clearDisplay();
            display_.clearFrameBuffer();
            display_.updateDisplay();
        };
        // If we're on the UiQueue currently, draw directly instead of dispatching
        if (DispatchGetUiQueue()->isInQueue()) {
            clearScreen();
        } else {
            DispatchGetUiQueue()->dispatch(clearScreen);
        }
    }
    DispatchShutdown();
}

void PowerManager::reboot() {
    beginShutdown();
    esp_restart();
}

void PowerManager::ulpDeepSleep(SleepReason reason) {
#if DISABLE_DEEP_SLEEP
    log_w("Deep sleep disabled. Not sleeping.");
    return;
#endif
    beginShutdown();
    deepSleepInternal(reason);
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

void PowerManager::deepSleepInternal(SleepReason reason) {
    log_d("Beginning shutdown sequence with reason %s", ToString(reason).c_str());

    bool syncOnClose = (reason == SleepReason::TempleClosed || reason == SleepReason::Inactivity) &&
                       !prefs_.noDeliverOnClose.get().value_or(false);
    if (syncOnClose) {
        // On temples close or inactivity we want to set a sync-on-close timer 30s after sleep
        // starts. This is mostly to save placemarks after reading
        if (reason == SleepReason::TempleClosed) {
            PreferencesStore::syncReason = SyncReason::OnClose;
            PreferencesStore::nextSyncTime = SolClock::getInstance().getEpochTime() + 30;
            log_d("Sync-on-close scheduled for %" PRIu64, PreferencesStore::nextSyncTime);
        } else if (reason == SleepReason::Inactivity && state_ != PowerState::ActiveOverTemp) {
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
    // Wait a moment for these last logs to go out (for the benefit of the ICT)
    vTaskDelay(pdMS_TO_TICKS(100));
    esp_deep_sleep_start();
#endif
}
