#pragma once

#include <ctime>
#include <freertos/FreeRTOS.h>
#include <freertos/event_groups.h>
#include <string>

#include "Storage/PreferencesStore.hpp"

class SolClock {
public:
    static auto getInstance() -> SolClock &;

    SolClock(PreferencesStore &prefs) : prefs_(prefs) { timeSyncEventGroup_ = xEventGroupCreate(); }
    SolClock(SolClock const &) = delete;

    void begin() { loadTimeZone(); }

    auto getTimeString() -> std::string;

    void getLocaltime(tm *timeinfo);

    void getClockTime(uint8_t &hours, uint8_t &minutes);

    static auto timeZoneString(int timeZone) -> std::string;
    auto timeZoneString() -> std::string;
    void setTimezone(int timeZone);

    void setClockTimeFromInternet();

    auto calculateNextSyncTime(time_t now) -> time_t;

    auto getEpochTime() -> time_t;

protected:
    EventGroupHandle_t timeSyncEventGroup_;
    PreferencesStore &prefs_;

    void loadTimeZone();
};
