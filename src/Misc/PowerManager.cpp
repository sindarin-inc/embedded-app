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
            deepSleep(SleepReason::UpdateUSBState);
            break;
        }
        case ESP_SLEEP_WAKEUP_COCPU_TRAP_TRIG:
            bootInfo_.reason = BootInfo::Reason::CoCpuTrap;
            log_e("ULP crashed");
            // Back to sleep
            deepSleep(SleepReason::UlpCrashed);
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
            if (bootInfo_.reason == BootInfo::Reason::PowerOn && USBConnected()) {
                // If this is a power-on boot, and USB is connected, we mark it as sleep reason
                // UpdateUSBState so that we will schedule a content sync.
                deepSleep(SleepReason::UpdateUSBState);
            } else {
                deepSleep(SleepReason::TempleClosedAtBoot);
            }
        }
        state_ = PowerState::Active;
        log_i("Beginning active boot");
    } else if (state_ == PowerState::Background) {
        auto now = SolClock::getInstance().getEpochTime();
        if (PreferencesStore::nextSyncTime && now >= PreferencesStore::nextSyncTime) {
            log_i("Time for background content sync (%" PRIu64 " (now) >= %" PRIu64 ")",
                  uint64_t(now), PreferencesStore::nextSyncTime);
        } else {
            log_i("Next sync time is %" PRIu64 ". Back to sleep.", PreferencesStore::nextSyncTime);
            deepSleep(SleepReason::WaitingForSync);
        }
    } else {
        log_w("Unexpectedly resuming boot");
    }
}

auto PowerManager::inactivityTimeoutMs() -> int {
    if (state_ == PowerState::ActiveUSBCharging) {
        if (PreferencesStore::paired == PairedStatus::Paired) {
            // Paired glasses turn off quickly with USB connected
            return 15 * 1000;
        } else {
            // Longer timeout for unpaired glasses in case users try to set-up the glasses while
            // connected to USB
            return 5 * 60 * 1000;
        }
    } else if (state_ == PowerState::ActiveOverTemp) {
        // Shutdown relatively quickly if we're too hot
        return 10 * 1000;
    } else {
        // Normal inactivity timeout is 10 minutes
        return 10 * 60 * 1000;
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
        inactivityTimer_.start(
            [this]() {
            log_i("Inactive! Close and open temple to wake");
            deepSleep(SleepReason::Inactivity);
            },
            inactivityTimeoutMs());
    }
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
        [this]() {
#if !DISABLE_POWER_MANAGEMENT
        DispatchGetCore1BackgroundQueue()->dispatch([this]() {
            // If USB is connected, disable operation unless we've disabled this behavior
            if (USBConnected() && !prefs_.allowUsageWhileCharging.get().value_or(false)) {
                log_i("USB connected, switching to ActiveUSBCharging");
                state_ = PowerState::ActiveUSBCharging;
                inactivityTimer_.setTimeoutMs(inactivityTimeoutMs());
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
    this->telemetry_.logMetric(TelemetryField::METRIC_SHUTDOWN, "PowerOff");

    beginShutdown();
    powerOffInternal();
}

void PowerManager::powerOffInternal() {
    log_i("Powering off");

    AllowPowerOff();

    esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_ALL);

    // If USB is disconnected, still allow a wakeup if the USB is plugged in
    enableUsbWakeup();

    esp_deep_sleep_start();
}

void PowerManager::loop() {
    battery.loop();
    if (battery.getDepleted()) {
        if (state_ == PowerState::Active || state_ == PowerState::ActiveForcedOn) {
            state_ = PowerState::ActiveBatteryDepleted;
            DispatchGetUiQueue()->dispatch([this]() {
                if (stateChangeCallback_ != nullptr) {
                    stateChangeCallback_(state_);
                }
                Delay(5000);
                powerOff();
            });
        } else {
            powerOff();
        }
    }
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
                deepSleep(SleepReason::TempleClosed);
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
        inactivityTimer_.setTimeoutMs(inactivityTimeoutMs());
        DispatchGetUiQueue()->dispatch([this]() {
            if (stateChangeCallback_ != nullptr) {
                stateChangeCallback_(state_);
            }
        });
    }

#endif
}

auto PowerManager::isTempleOpen(bool &open) -> esp_err_t {
#if USE_MMC5603
    auto err = magSensor_.readXZ8Bit(lastMagReading_);
    if (err != ESP_OK) {
        log_e("Failed to read magnetometer");
        return err;
    }
    open = IsTempleOpen(lastMagReading_.x, lastMagReading_.z);
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
            PreferencesStore::isCharged = true;
            const uint32_t delayPeriod = 30 * 60; // 30 min delay upon reaching 95% battery level
            time_t currentTime = SolClock::getInstance().getEpochTime();

            if (PreferencesStore::ledChargeTime == -1) {
                // If the battery is fully charged, start the timer before setting the LED to green
                PreferencesStore::ledChargeTime = currentTime + delayPeriod;
                // log_i("Battery fully charged, starting timer for %lu seconds", delayPeriod);
            }

            if (PreferencesStore::ledChargeTime == 0 ||
                currentTime >= PreferencesStore::ledChargeTime) {
                // If the battery is fully charged and a half hour has passed, turn on the LED
                DigitalWrite(GREEN_LED, LED_ON);
                DigitalWrite(RED_LED, LED_OFF);
                // log_i("Battery fully charged, turning on green LED");
            } else {
                DigitalWrite(GREEN_LED, LED_OFF);
                DigitalWrite(RED_LED, LED_ON);
                // log_i("Battery fully charged, waiting for %" PRId64 " seconds",
                //       PreferencesStore::ledChargeTime - currentTime);
            }
        } else if (percentage > 85 && PreferencesStore::isCharged) {
            // Keep the LED green if it's charged and between 85-95%
            DigitalWrite(GREEN_LED, LED_ON);
            DigitalWrite(RED_LED, LED_OFF);
            // log_i("Battery level is above 85%%, keeping green LED on.");
        } else {
            PreferencesStore::isCharged = false;
            PreferencesStore::ledChargeTime = -1;
            DigitalWrite(GREEN_LED, LED_OFF);
            DigitalWrite(RED_LED, LED_ON);
            // log_d("Battery not fully charged, turning on red LED. Percentage: %d%%",
            //       int(percentage));
        }
        gpio_hold_en((gpio_num_t)GREEN_LED);
        gpio_hold_en((gpio_num_t)RED_LED);
        gpio_deep_sleep_hold_en();
    } else {
        // log_d("USB disconnected, resetting charged LED");
        PreferencesStore::ledChargeTime = 0; // reset the timer as the USB is disconnected
        PreferencesStore::isCharged = false; // reset the isCharged flag as the USB is disconnected
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
        state_ == PowerState::ActiveUSBCharging || state_ == PowerState::ActiveForcedOn ||
        state_ == PowerState::ActiveBatteryDepleted) {
        lightingController_.disableLighting(200);
        // Delay to see the fade.
        // TODO: Use a callback instead
        Delay(200);
        auto clearScreen = [this]() {
            display_.clearDisplay();
            display_.clearFrameBuffer();
            display_.updateDisplay();
            PreferencesStore::screenClearedAtShutdown = true;
        };
        // If we're on the UiQueue currently, draw directly instead of dispatching
        if (DispatchGetUiQueue()->isInQueue()) {
            clearScreen();
        } else {
            DispatchGetUiQueue()->dispatch(clearScreen);
        }
    }
    DispatchShutdown();

    // Make sure the ambient light sensor is off so it's not using power while in deep sleep
    lightingController_.disableLighting(0);

    prefs_.end();

    esp_err_t err = DeinitializePlatform();
    if (err != ESP_OK) {
        log_e("Failed to deinit platform with err %s", esp_err_to_name(err));
    }

    err = HardwareDeinit();
    if (err != ESP_OK) {
        log_e("Failed to deinit hardware with err %s", esp_err_to_name(err));
    }
}

void PowerManager::reboot() {
    beginShutdown();
    esp_restart();
}

void PowerManager::scheduleSync(SyncReason reason, int time) {
    PreferencesStore::syncReason = reason;
    PreferencesStore::nextSyncTime = time;
    log_d("Sync on %s scheduled for %" PRIu64, ToString(PreferencesStore::syncReason),
          PreferencesStore::nextSyncTime);
}

void PowerManager::deepSleep(SleepReason reason) {
    this->telemetry_.logMetric(TelemetryField::METRIC_SHUTDOWN, ToString(reason));

#if DISABLE_DEEP_SLEEP
    log_w("Deep sleep disabled. Not sleeping.");
    return;
#endif

    // This is the block of code that calculates the time for the next WiFi sync. All code that
    // sets PreferencesStore::syncReason / PreferencesStore::nextSyncTime should happen here
    auto now = SolClock::getInstance().getEpochTime();
    switch (reason) {
    case SleepReason::Inactivity:
        if (!prefs_.noDeliverOnClose.get().value_or(false) &&
            state_ != PowerState::ActiveOverTemp) {
            scheduleSync(SyncReason::OnInactivity, now + 30);
        }
        break;
    case SleepReason::TempleClosed:
        if (!prefs_.noDeliverOnClose.get().value_or(false)) {
            scheduleSync(SyncReason::OnClose, now + 30);
        }
        break;
    case SleepReason::UpdateUSBState:
        if (USBConnected()) {
            scheduleSync(SyncReason::UsbConnected, now + 30);
        } else if (PreferencesStore::syncReason == SyncReason::UsbConnected) {
            // USB is disconnected now, cancel pending UsbConnected sync
            PreferencesStore::syncReason = SyncReason::Manual;
            PreferencesStore::nextSyncTime = 0;
        }
        break;
    case SleepReason::SyncDone:
        scheduleSync(SyncReason::Periodic, SolClock::getInstance().calculateNextSyncTime(now));
        break;
    case SleepReason::Unspecified:
    case SleepReason::TempleClosedAtBoot:
    case SleepReason::UlpCrashed:
    case SleepReason::WaitingForSync:
        break;
    }

    if (PreferencesStore::nextSyncTime <= now) {
        // If there is no other sync scheduled, figure out the right time for the next sync
        // The only time we expect this to happen is if USB is connected then disconnected in < 30s
        log_w("No sync scheduled, calculating next sync time.");
        if (!prefs_.started()) {
            prefs_.begin();
        }
        scheduleSync(SyncReason::Periodic, SolClock::getInstance().calculateNextSyncTime(now));
    }

    // If we don't know the paired status, load it from NVS so we can know whether to power off
    if (PreferencesStore::paired == PairedStatus::Unknown) {
        if (!prefs_.started()) {
            prefs_.begin();
        }
    }

    beginShutdown();
    deepSleepInternal(reason);
}

auto ToString(SleepReason reason) -> const char * {
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
        log_e("Unknown SleepReason: %d", static_cast<int>(reason));
        return "Unknown";
    }
}

auto PowerManager::enableUsbWakeup() -> esp_err_t {
    esp_err_t err;
    if (USBConnected()) {
        err = esp_sleep_enable_ext0_wakeup((gpio_num_t)USB_CONNECTED_PIN, 0);
        if (err != ESP_OK) {
            log_e("error enabling USB_CONNECTED_PIN low wakeup from deep sleep %d", err);
            return err;
        }
    } else {
        err = esp_sleep_enable_ext0_wakeup((gpio_num_t)USB_CONNECTED_PIN, 1);
        if (err != ESP_OK) {
            log_e("error enabling USB_CONNECTED_PIN high wakeup from deep sleep %d", err);
            return err;
        }
    }
    return ESP_OK;
}

void PowerManager::deepSleepInternal(SleepReason reason) {
    log_d("deepSleepInternal with reason %s", ToString(reason));

    // If these glasses haven't been paired (or something went wrong with NVS and we don't know what
    // state their in), then allow them to power back off.
    if (PreferencesStore::paired == PairedStatus::Unpaired ||
        PreferencesStore::paired == PairedStatus::Unknown) {
        log_w("Unpaired glasses, allowing power off");
        AllowPowerOff();
    }

#if SIMULATOR
    log_i("deepSleepInternal");
#else

    esp_err_t err;

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
        powerOffInternal();
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
        powerOffInternal();
        return;
    }

    err = esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_PERIPH, ESP_PD_OPTION_ON);
    if (err != ESP_OK) {
        log_e("error enabling ESP_PD_DOMAIN_RTC_PERIPH power domain %d", err);
        powerOffInternal();
        return;
    }

    err = esp_sleep_enable_ulp_wakeup();
    if (err != ESP_OK) {
        log_e("error enabling ulp wakeup from deep sleep %d", err);
        powerOffInternal();
        return;
    }

    // Enable a wakeup on the USB_CONNECTED_PIN
    err = enableUsbWakeup();
    if (err != ESP_OK) {
        powerOffInternal();
        return;
    }

    uint64_t sleepTime = UINT64_MAX;

    // When USB is connected, we want to wake up minutely and check the battery level (to know
    // when to turn the LED to green), otherwise we cant wait until the next content sync.
    if (USBConnected()) {
        sleepTime = 60 * uint64_t(1000000);
    }

    // Otherwise we want to wait until we want to run our next content sync if that's sooner
    // than what we've already set
    auto now = SolClock::getInstance().getEpochTime();
    if (now < PreferencesStore::nextSyncTime) {
        sleepTime = std::min(sleepTime, (PreferencesStore::nextSyncTime - now) * uint64_t(1000000));
    }

    if (sleepTime != UINT64_MAX) {
        err = esp_sleep_enable_timer_wakeup(sleepTime);
        if (err != ESP_OK) {
            log_e("error enabling timer wakeup for deep sleep %d", err);
        } else {
            log_d("Sleeping for %" PRIu64 " seconds", sleepTime / 1000000);
        }
    } else {
        log_w("No scheduled wakeup (nextSyncTime=%" PRIu64 ", now=%" PRIu64 ")",
              PreferencesStore::nextSyncTime, uint64_t(now));
    }

    esp_deep_sleep_start();
#endif
}
