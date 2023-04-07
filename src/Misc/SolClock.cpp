#include "SolClock.hpp"

#if !SIMULATOR
#include <esp_log.h>
#include <esp_sntp.h>
#endif

#include <cinttypes>

#include "../Communications/WiFi.hpp"
#include "../Misc/StringUtil.hpp"

extern SolClock solClock;

bool useSimulatorTestTime = false;
int simulatorTestHours = 0;
int simulatorTestMinutes = 0;

auto SolClock::getInstance() -> SolClock & { return solClock; }

auto SolClock::getTimeString() -> std::string {
    uint8_t hours, minutes;

    getClockTime(hours, minutes);

    if (hours == 0) {
        hours = 12;
    }
    if (hours > 12) {
        hours -= 12;
    }
    return StringFormat("%d:%02d", hours, minutes);
}

void SolClock::loadTimeZone() {
    std::string timeZoneString;
    switch (prefs_.solTimeZone.get().value_or(0)) {
    case -8:
        timeZoneString = "PST8PDT";
        break;
    case -7:
        timeZoneString = "MST7MDT";
        break;
    case -6:
        timeZoneString = "CST6CDT";
        break;
    case -5:
        timeZoneString = "EST5EDT";
        break;
    case 0:
    default:
        timeZoneString = "UTC";
        break;
    }
    log_d("Using timezone %s", timeZoneString.c_str());
    setenv("TZ", timeZoneString.c_str(), 1);
    tzset();
}

auto SolClock::timeZoneString(int timeZone) -> std::string {
    switch (timeZone) {
    case -8:
        return "Pacific Time";
    case -7:
        return "Mountain Time";
    case -6:
        return "Central Time";
    case -5:
        return "Eastern Time";
    case 0:
        return "UTC - Not Set";
    default:
        return "Unknown";
    }
}

auto SolClock::timeZoneString() -> std::string {
    return timeZoneString(prefs_.solTimeZone.get().value_or(0));
}

void SolClock::setTimezone(int timeZone) {
    prefs_.solTimeZone.set(timeZone);
    loadTimeZone();
}

void SolClock::getLocaltime(tm *timeinfo) {
    time_t now = time(nullptr);
    localtime_r(&now, timeinfo);
}

void SolClock::getClockTime(uint8_t &hours, uint8_t &minutes) {
#if SIMULATOR
    if (useSimulatorTestTime) {
        hours = simulatorTestHours;
        minutes = simulatorTestMinutes;
        return;
    }
#endif
    struct tm timeinfo;
    getLocaltime(&timeinfo);
    hours = timeinfo.tm_hour;
    minutes = timeinfo.tm_min;
}

void SolClock::setClockTimeFromInternet() {
#if !SIMULATOR

    // Configure the SNTP client
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 1, 0)
    esp_sntp_setoperatingmode(SNTP_OPMODE_POLL);
    esp_sntp_setservername(0, "time.google.com");
    esp_sntp_init();
#else
    sntp_setoperatingmode(SNTP_OPMODE_POLL);
    sntp_setservername(0, "time.google.com");
    sntp_init();
#endif

    // Wait for the SNTP client to get the time
    time_t now = 0;
    struct tm timeinfo {};

    sntp_set_time_sync_notification_cb([](struct timeval *tv) {
        log_i("Time synchronized");
        xEventGroupSetBits((SolClock::getInstance().timeSyncEventGroup_), 1);
    });

    // Wait for the event
    EventBits_t bits =
        xEventGroupWaitBits(timeSyncEventGroup_, 1, true, true, pdMS_TO_TICKS(10000));

    if (bits == 0) {
        log_e("Failed to sync time");
    } else {
        now = time(nullptr);
        localtime_r(&now, &timeinfo);
    }

#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 1, 0)
    esp_sntp_stop();
#else
    sntp_stop();
#endif

    // Print the time as a string
    char strftimeBuf[64];
    size_t size = strftime(strftimeBuf, sizeof(strftimeBuf), "%c", &timeinfo);
    if (size == 0) {
        log_e("strftime failed");
    }
    log_i("The current date/time is: %s (%" PRIu64 ")", strftimeBuf, now);
#else
    log_i("Not setting clock time from internet in simulator");
#endif
}

auto SolClock::calculateNextSyncTime(time_t now) -> time_t {
    // Current local time as a tm struct
    struct tm timeinfo;
    localtime_r(&now, &timeinfo);

    // Get the morning and evening news hours
    int morningNews = prefs_.morningNewspaperTime.get().value_or(0);
    int eveningNews = prefs_.eveningNewspaperTime.get().value_or(0);

    // If otherwise unselected, let's force a sync at 6pm
    if (morningNews == 0 && eveningNews == 0) {
        eveningNews = 18;
    }

    // Create tm structs for the morning and evening events
    struct tm morningEvent = timeinfo;
    morningEvent.tm_hour = morningNews;
    morningEvent.tm_min = 0;
    morningEvent.tm_sec = 0;

    struct tm eveningEvent = timeinfo;
    eveningEvent.tm_hour = eveningNews;
    eveningEvent.tm_min = 0;
    eveningEvent.tm_sec = 0;

    // Calculate the epoch time for both events
    time_t morningEventEpoch = mktime(&morningEvent);
    time_t eveningEventEpoch = mktime(&eveningEvent);

    // Adjust for the next day if the events have already occurred today
    if (morningEventEpoch <= now) {
        morningEvent.tm_mday += 1;
        morningEventEpoch = mktime(&morningEvent);
    }
    if (eveningEventEpoch <= now) {
        eveningEvent.tm_mday += 1;
        eveningEventEpoch = mktime(&eveningEvent);
    }

    // Determine the next event
    time_t nextEventEpoch;
    char strftimeBuf[64];
    if (morningNews != 0 && morningEventEpoch <= eveningEventEpoch) {
        nextEventEpoch = morningEventEpoch;
        size_t size = strftime(strftimeBuf, sizeof(strftimeBuf), "%c", &morningEvent);
        if (size == 0) {
            log_e("strftime failed");
        }
    } else {
        nextEventEpoch = eveningEventEpoch;
        size_t size = strftime(strftimeBuf, sizeof(strftimeBuf), "%c", &eveningEvent);
        if (size == 0) {
            log_e("strftime failed");
        }
    }
    log_i("Next sync time is: %s (%" PRIu64 "s from now)", strftimeBuf,
          (int64_t)nextEventEpoch - now);

    return nextEventEpoch;
}

auto SolClock::getEpochTime() -> time_t { return time(nullptr); }
