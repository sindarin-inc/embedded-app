#pragma once

#include "Misc/Battery.hpp"
#include "Misc/InactivityTimer.hpp"
#include "Misc/SolClock.hpp"
#include "Sensors/LightingController.hpp"
#include "Sensors/TempleCloseSensor.hpp"
#include "Storage/PreferencesStore.hpp"

#if !DISABLE_POWER_MANAGEMENT
#include <esp_pm.h>
#endif

enum class PowerState {
    Booting,
    // Screen lighting enabled ~10-60mA, inavtivity timer running.
    Active,
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
                 LightingController &lightingController)
        : battery(fileSystem, prefs), prefs_(prefs), lightingController_(lightingController) {}
    PowerManager(const PowerManager &) = delete;

    // Runs first thing upon startup to figure out what power mode we're in.
    void wakeupCheck(SolClock &clock);

    auto begin() -> esp_err_t;

    auto state() -> PowerState { return state_; }

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

    // Exposed so we can check this peripheral as a part of the power-on self-test
    auto checkMagnetometerProductId() -> bool {
#if USE_MMC5603
        return templeCloseSensor_.checkProductId();
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
    LightingController &lightingController_;
    InactivityTimer inactivityTimer_;
    TempleCloseSensor templeCloseSensor_;
#if !DISABLE_POWER_MANAGEMENT
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 1, 0)
    esp_pm_config_t pmConfig_;
#else
    esp_pm_config_esp32s3_t pmConfig_;
#endif
#endif
    void updateState();

    // Check if the USB cable is plugged in and check the battery state. Turn on the LED to either
    // red or green depending on the charge of the battery.
    void checkUsb();
};
