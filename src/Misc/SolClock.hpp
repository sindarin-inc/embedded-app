#include <ctime>
#include <string>

#include "Storage/PreferencesStore.hpp"

class SolClock {
public:
    static auto getInstance() -> SolClock &;

    SolClock(PreferencesStore &prefs) : prefs_(prefs) {}

    auto getTimeString() -> std::string;

    void getClockTime(uint8_t &hours, uint8_t &minutes);

    void setClockTimeFromInternet();

protected:
    PreferencesStore &prefs_;
};