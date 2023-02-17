#include "Storage/PreferencesStore.hpp"

class PowerManager {
public:
    static auto getInstance() -> PowerManager &;

    PowerManager(PreferencesStore &prefs) : prefs_(prefs) {}

    void PowerOff();

protected:
    PreferencesStore &prefs_;
};