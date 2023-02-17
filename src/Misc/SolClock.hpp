#include <ctime>
#include <string>

#include "Storage/PreferencesStore.hpp"

class SolClock {
public:
    static auto getInstance() -> SolClock &;

    SolClock(PreferencesStore &prefs) : prefs_(prefs) {}

    void GetClockTime(uint8_t &hours, uint8_t &minutes);

    void SetClockTimeFromInternet();

protected:
    PreferencesStore &prefs_;
};