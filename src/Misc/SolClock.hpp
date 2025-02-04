#pragma once

#include <ctime>
#include <freertos/FreeRTOS.h>
#include <freertos/event_groups.h>
#include <string>
#include <unordered_map>

#include "Misc/CancelFlag.hpp"
#include "Storage/PreferencesStore.hpp"

class SolClock {
public:
    static auto getInstance() -> SolClock &;

    SolClock(PreferencesStore &prefs) : timeSyncEventGroup_(xEventGroupCreate()), prefs_(prefs) {}
    SolClock(SolClock const &) = delete;
    ~SolClock() { vEventGroupDelete(timeSyncEventGroup_); }

    auto isValidTime() -> bool { return getEpochTime() > 1000000000; }

    void begin() { loadTimeZone(); }

    auto getTimeString() -> std::string;

    void getLocaltime(tm *timeinfo);

    void getClockTime(uint8_t &hours, uint8_t &minutes);

    auto ianaToPosix(const std::string &ianaTimezone) -> std::string;

    auto ianaTimezoneString() -> std::string;
    auto posixTimezoneString() -> std::string;
    void setTimezone(const std::string &ianaTimezone);

    auto setClockTimeFromInternet(bool useWifi = true, bool useBle = false,
                                  const std::shared_ptr<CancelFlag> &cancel = nullptr) -> bool;

    auto calculateNextSyncTime(time_t now) -> time_t;

    auto getEpochTime() -> time_t;

protected:
    EventGroupHandle_t timeSyncEventGroup_;
    PreferencesStore &prefs_;

    void loadTimeZone();
    auto configTimeSNTP() -> bool;
};
