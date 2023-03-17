#include "Misc/InactivityTimer.hpp"
#include "Storage/PreferencesStore.hpp"

#if !DISABLE_POWER_MANAGEMENT
#include <esp_pm.h>
#endif

class PowerManager {
public:
    static auto getInstance() -> PowerManager &;

    PowerManager(PreferencesStore &prefs) : prefs_(prefs) {}

    auto begin() -> esp_err_t;

    void updateState();

    void powerOff();

    void feedInactivityTimer() { inactivityTimer_.feed(); }

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