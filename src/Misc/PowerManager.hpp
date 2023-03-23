#include "Misc/Battery.hpp"
#include "Misc/InactivityTimer.hpp"
#include "Storage/PreferencesStore.hpp"

#if !DISABLE_POWER_MANAGEMENT
#include <esp_pm.h>
#endif

class PowerManager {
public:
    static auto getInstance() -> PowerManager &;

    PowerManager(FileSystem &fileSystem, PreferencesStore &prefs)
        : battery(fileSystem, prefs), prefs_(prefs) {}
    PowerManager(const PowerManager &) = delete;

    auto begin() -> esp_err_t;

    void updateState();

    void powerOff();

    void feedInactivityTimer() { inactivityTimer_.feed(); }

    void loop() { battery.loop(); }

    Battery battery;

protected:
    PreferencesStore &prefs_;
    InactivityTimer inactivityTimer_;
#if !DISABLE_POWER_MANAGEMENT
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 1, 0)
    esp_pm_config_t pmConfig_;
#else
    esp_pm_config_esp32s3_t pmConfig_;
#endif
#endif
};