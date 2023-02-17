#include "Storage/PreferencesStore.hpp"

class PowerManager {
public:
    static auto getInstance() -> PowerManager &;

    PowerManager(PreferencesStore &prefs) : prefs_(prefs) {}

    void powerOff();

protected:
    PreferencesStore &prefs_;
};