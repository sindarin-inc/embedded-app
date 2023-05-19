#pragma once

#include <utility>

#include "Misc/Battery.hpp"
#include "Misc/InactivityTimer.hpp"
#include "Misc/SolClock.hpp"
#include "Sensors/LightingController.hpp"
#include "Sensors/Mmc5603.hpp"
#include "Storage/PreferencesStore.hpp"
#if EINKET013TT1
#include "Displays/DisplayEinkET013TT1.hpp"
#endif

#if !DISABLE_POWER_MANAGEMENT
#include <esp_pm.h>
#endif

static constexpr auto MAX_OPERATIONAL_TEMPERATURE_C = 50;

enum class PowerState {
    Booting,
    // Screen lighting enabled ~10-60mA, inavtivity timer running.
    Active,
    // Screen lighting enabled ~10-60mA, inactivity timer at 10s, UI disabled
    ActiveUSBCharging,
    // Screen lighting enabled ~10-60mA, inactivity timer at 10s, UI disabled
    ActiveOverTemp,
    // Screens/lighting disabled ~6-7mA, inactivity timer disabled.
    Background,
    // Deep sleep with ULP magnetometer and periodic wakeups ~100uA
    // Included for completeness not used in main CPU code currently
    Sleeping,
    // Forced on (screen lighting enabled and MCU still running) while temples closed
    // Same as Active but no turning off when the temples are closed until
    // explictly taken out of this state or put to deep sleep.
    ForcedOn,
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

class PowerManager {
public:
    static auto getInstance() -> PowerManager &;

    // Power manager initialization need to be as simple as possible
    PowerManager(FileSystem &fileSystem, PreferencesStore &prefs,
                 LightingController &lightingController, DisplaySystem &display)
        : battery(fileSystem, prefs), prefs_(prefs), lightingController_(lightingController),
          display_(display), magSensor_(0) {}

    PowerManager(const PowerManager &) = delete;

    // Runs first thing upon startup to figure out what power mode we're in.
    void wakeupCheck(SolClock &clock);

    auto begin() -> esp_err_t;

    auto forceOn() -> void { state_ = PowerState::ForcedOn; }
    auto disableForceOn() -> void { state_ = PowerState::Active; }

    auto state() -> PowerState { return state_; }

    void setStateChangeCallback(std::function<void(PowerState)> callback) {
        stateChangeCallback_ = std::move(callback);
    }

    // Power lifecycle functions
    void reboot();
    void ulpDeepSleep(SleepReason reason = SleepReason::Unspecified);
    // Fully power down the device (assuming USB is disconnected and left temple closed)
    // Note: This should be used sparingly as it's unintuitive to the user if you get into
    // this state and USB is connected, since the deep-sleep-forever won't exit until usb is
    // disconnected and the left temple is closed
    void powerOff();

    void feedInactivityTimer() { inactivityTimer_.feed(); }

    void loop();

    void releaseLED();

    // Exposed so we can check this peripheral as a part of the power-on self-test
    auto checkMagnetometerProductId() -> bool {
#if USE_MMC5603
        auto mmcProductId = magSensor_.readProductId();
        if (mmcProductId != 0x10) {
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

    // Exposed in public: for easier refactoring, but we should consider only exposing the necessary
    // methods
    Battery battery;

protected:
    PreferencesStore &prefs_;
    PowerState state_ = PowerState::Booting;
    std::function<void(PowerState)> stateChangeCallback_;
    LightingController &lightingController_;
    DisplaySystem &display_;
    InactivityTimer inactivityTimer_;
    Mmc5603 magSensor_;
    bool templeOpen_ = false;
#if !DISABLE_POWER_MANAGEMENT
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 1, 0)
    esp_pm_config_t pmConfig_;
#else
    esp_pm_config_esp32s3_t pmConfig_;
#endif
#endif
    void updateState();

    auto isTempleOpen(bool &open) -> esp_err_t;

    // Check if the USB cable is plugged in and check the battery state. Turn on the LED to either
    // red or green depending on the charge of the battery.
    void checkUsb();

    void waitForQueuesThenDeepSleep(SleepReason reason);
};
