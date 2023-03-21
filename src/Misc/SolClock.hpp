#include <ctime>
#include <freertos/FreeRTOS.h>
#include <freertos/event_groups.h>
#include <string>

#include "Storage/PreferencesStore.hpp"

class SolClock {
public:
    static auto getInstance() -> SolClock &;
    // Delete the copy constructor
    SolClock(SolClock const &) = delete;

    SolClock(PreferencesStore &prefs) : prefs_(prefs) { timeSyncEventGroup_ = xEventGroupCreate(); }

    auto getTimeString() -> std::string;

    void getClockTime(uint8_t &hours, uint8_t &minutes);

    void setClockTimeFromInternet();

protected:
    EventGroupHandle_t timeSyncEventGroup_;
    PreferencesStore &prefs_;
};
