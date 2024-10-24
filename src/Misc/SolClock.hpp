#pragma once

#include <ctime>
#include <freertos/FreeRTOS.h>
#include <freertos/event_groups.h>
#include <string>
#include <unordered_map>

#include "Storage/PreferencesStore.hpp"

class SolClock {
public:
    static auto getInstance() -> SolClock &;

    SolClock(PreferencesStore &prefs) : prefs_(prefs) {}
    SolClock(SolClock const &) = delete;
    ~SolClock() {
        if (timeSyncEventGroup_ != nullptr) {
            vEventGroupDelete(timeSyncEventGroup_);
        }
    }

    auto isValidTime() -> bool { return getEpochTime() > 1000000000; }

    void begin() {
        timeSyncEventGroup_ = xEventGroupCreate();
        loadTimeZone();
    }

    auto getTimeString() -> std::string;

    void getLocaltime(tm *timeinfo);

    void getClockTime(uint8_t &hours, uint8_t &minutes);

    auto ianaToPosix(const std::string &ianaTimezone) -> std::string;

    auto ianaTimezoneString() -> std::string;
    auto posixTimezoneString() -> std::string;
    void setTimezone(const std::string &ianaTimezone);

    auto setClockTimeFromInternet() -> bool;

    auto calculateNextSyncTime(time_t now) -> time_t;

    auto getEpochTime() -> time_t;

protected:
    EventGroupHandle_t timeSyncEventGroup_ = nullptr;
    PreferencesStore &prefs_;

    void loadTimeZone();
};
