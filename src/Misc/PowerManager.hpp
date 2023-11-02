#pragma once

#include <esp_sleep.h>
#include <esp_system.h>
#include <rom/rtc.h>
#include <utility>

#include "Misc/Battery.hpp"
#include "Misc/InactivityTimer.hpp"
#include "Misc/SolClock.hpp"
#include "Misc/Telemetry.hpp"
#include "Sensors/LightingController.hpp"
#include "Sensors/Mmc5603.hpp"
#include "Storage/PreferencesStore.hpp"
#if EINKET013TT1
#include "Displays/DisplayEinkET013TT1.hpp"
#endif

#if !DISABLE_POWER_MANAGEMENT
#include <esp_pm.h>
#endif

static constexpr auto MAX_OPERATIONAL_TEMPERATURE_C = 50.0f;

enum class PowerState {
    Booting,
    // Screen lighting enabled ~10-60mA, inavtivity timer running.
    Active,
    // Forced on (screen lighting enabled and MCU still running) while temples closed
    // Same as Active but no turning off when the temples are closed until
    // explictly taken out of this state or put to deep sleep.
    ActiveForcedOn,
    // Screen lighting enabled ~10-60mA, inactivity timer at 15s, UI disabled
    ActiveUSBCharging,
    // Screen lighting enabled ~10-60mA, inactivity timer at 10s, UI disabled
    ActiveOverTemp,
    // Screen lighting enabled ~10-60mA, inactivity timer disabled (will force powerOff 5s after
    // showing the depleted UI), UI disabled
    ActiveBatteryDepleted,
    // On the way to shutting down
    ShuttingDown,
    // Screens/lighting disabled ~6-7mA, inactivity timer disabled.
    Background,
    // Deep sleep with ULP magnetometer and periodic wakeups ~100uA
    // Included for completeness not used in main CPU code currently
    Sleeping,
    // Fully off (deep sleep) ~22uA
    Off,
};

// All the reasons we might go to deep sleep w/ ULP.
enum class SleepReason {
    Unspecified,
    Inactivity,
    TempleClosed,
    TempleClosedAtBoot,
    SyncDone,
    UlpCrashed,
    UpdateUSBState,
    WaitingForSync,
};

auto ToString(SleepReason reason) -> const char *;

struct BootInfo {
    enum class Reason {
        Unknown = 0,

        // Mirrored from esp_reset_reason_t
        PowerOn,
        Sw,
        Panic,
        IntWdt,
        TaskWdt,
        Wdt,
        Brownout,

        // Lower level from RESET_REASON (from rtc_get_reset_reason)
        UartChipReset,

        // Deep sleep wakeup reasons (derived from esp_sleep_wakeup_cause_t)
        UsbPlug, // AKA Ext0
        Timer,
        UlpMag,
        CoCpuTrap,
    };

    Reason reason = Reason::Unknown;

    // Raw values from the various reset reasons (for debugging unknown/unexpected conditions)
    esp_reset_reason_t resetReason = ESP_RST_UNKNOWN;
    RESET_REASON rtcResetReason = NO_MEAN;
    esp_sleep_wakeup_cause_t sleepWakeupCause = ESP_SLEEP_WAKEUP_UNDEFINED;

    auto humanStr() -> std::string {
        switch (reason) {
        case Reason::UartChipReset:
            return "USB Uart Reset";
        case Reason::PowerOn:
            return "Power On";
        case Reason::Sw:
            return "Software Reset";
        case Reason::Panic:
            return "Software Panic";
        case Reason::IntWdt:
            return "Interrupt Watchdog";
        case Reason::TaskWdt:
            return "Task Watchdog";
        case Reason::Timer:
            return "Deep Sleep Timer";
        case Reason::UlpMag:
            return "ULP Mag Sensor";
        case Reason::UsbPlug:
            return "USB Plugged/Unplugged";
        case Reason::CoCpuTrap:
            return "ULP Crashed";
        case Reason::Brownout:
            return "Brownout";
        case Reason::Wdt:
            return "General Watchdog";
        default:
            return StringFormat(
                "Unknown (resetReason: %d, rtcResetReason: %d, sleepWakeupCause: %d)", resetReason,
                rtcResetReason, sleepWakeupCause);
        }
    }

    auto telemetryStr() -> std::string {
        switch (reason) {
        case Reason::UartChipReset:
            return "UartChipReset";
        case Reason::PowerOn:
            return "PowerOn";
        case Reason::Sw:
            return "Software";
        case Reason::Panic:
            return "Panic";
        case Reason::IntWdt:
            return "IntWdt";
        case Reason::TaskWdt:
            return "TaskWdt";
        case Reason::Timer:
            return "Timer";
        case Reason::UlpMag:
            return "UlpMag";
        case Reason::UsbPlug:
            return "UsbPlug";
        case Reason::CoCpuTrap:
            return "CoCpuTrap";
        case Reason::Brownout:
            return "Brownout";
        case Reason::Wdt:
            return "Wdt";
        default:
            return StringFormat("Unknown (%d,%d,%d)", resetReason, rtcResetReason,
                                sleepWakeupCause);
        }
    }
};

class PowerManager {
public:
    static auto getInstance() -> PowerManager &;

    // Power manager initialization need to be as simple as possible
    PowerManager(FileSystem &fileSystem, PreferencesStore &prefs,
                 LightingController &lightingController, DisplaySystem &display,
                 Telemetry &telemetry)
        : battery(fileSystem, prefs, telemetry), prefs_(prefs),
          lightingController_(lightingController), display_(display), telemetry_(telemetry),
          magSensor_(0) {}

    PowerManager(const PowerManager &) = delete;

    // Runs first thing upon startup to figure out what power mode we're in.
    void wakeupCheck(SolClock &clock);

    auto begin() -> esp_err_t;

    auto forceOn() -> void { state_ = PowerState::ActiveForcedOn; }
    auto disableForceOn() -> void { state_ = PowerState::Active; }

    auto state() -> PowerState { return state_; }

    void setStateChangeCallback(std::function<void(PowerState)> callback) {
        stateChangeCallback_ = std::move(callback);
    }

    void setBeginShutdownCallback(const std::function<void()> &callback) {
        beginShutdownCallback_ = callback;
    }

    // Power lifecycle functions
    void reboot();
    void deepSleep(SleepReason reason);
    // Fully power down the device (assuming USB is disconnected and left temple closed)
    // Note: This should be used sparingly as it's unintuitive to the user if you get into
    // this state and USB is connected, since the deep-sleep-forever won't exit until usb is
    // disconnected and the left temple is closed
    void powerOff();

    void feedInactivityTimer() { inactivityTimer_.feed(); }
    void disableInactivityTimer() { inactivityTimer_.pause(); }
    void enableInactivityTimer() { inactivityTimer_.resume(); }

    void loop();

    void releaseLED();

    // Exposed so we can check this peripheral as a part of the power-on self-test
    auto checkMagnetometerProductId() -> bool {
#if USE_MMC5603
        uint8_t mmcProductId;
        auto err = magSensor_.readProductId(&mmcProductId);
        if (err != ESP_OK) {
            log_e("MMC5603 magnetometer not found (error: %s)", esp_err_to_name(err));
            return false;
        } else if (mmcProductId != 0x10) {
            log_e("MMC5603 magnetometer not found (productId: 0x%02X)", mmcProductId);
            return false;
        } else {
            log_i("MMC5603 magnetometer found (productId: 0x%02X)", mmcProductId);
            return true;
        }
#else
        return true;
#endif
    }

    auto getBootInfo() -> BootInfo { return bootInfo_; }
    auto getLastMagReading() -> Xyz8bit { return lastMagReading_; }

    // Exposed in public: for easier refactoring, but we should consider only exposing the necessary
    // methods
    Battery battery;

protected:
    PreferencesStore &prefs_;
    PowerState state_ = PowerState::Booting;
    std::function<void(PowerState)> stateChangeCallback_;
    std::function<void()> beginShutdownCallback_;
    LightingController &lightingController_;
    DisplaySystem &display_;
    InactivityTimer inactivityTimer_;
    Telemetry &telemetry_;
    Mmc5603 magSensor_;
    bool templeOpen_ = false;
    BootInfo bootInfo_;
    Xyz8bit lastMagReading_ = {};
#if !DISABLE_POWER_MANAGEMENT
    esp_pm_config_t pmConfig_;
#endif
    void updateState();

    auto isTempleOpen(bool &open) -> esp_err_t;

    // Check if the USB cable is plugged in and check the battery state. Turn on the LED to either
    // red or green depending on the charge of the battery.
    void checkUsb();

    void beginShutdown();

    void deepSleepInternal(SleepReason reason);

    void scheduleSync(SyncReason reason, int timeFromNow);

    // Fully power down the device (assuming USB is disconnected and left temple closed)
    // Note: This should be used sparingly as it's unintuitive to the user if you get into
    // this state and USB is connected, since the deep-sleep-forever won't exit until usb is
    // disconnected and the left temple is closed
    void powerOffInternal();

    auto enableUsbWakeup() -> esp_err_t;

    auto inactivityTimeoutMs() -> int;
};
