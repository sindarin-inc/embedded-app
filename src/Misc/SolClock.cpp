#include "SolClock.hpp"

#if !SIMULATOR
#include <esp_log.h>
#include <esp_sntp.h>
#endif

#include <cinttypes>

#include "../Communications/WiFi.hpp"
#include "../Misc/StringUtil.hpp"
#include "../Misc/Timezones.hpp"

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
    log_d("Loading timezone %s=%s", ianaTimezoneString().c_str(), posixTimezoneString().c_str());
    setenv("TZ", posixTimezoneString().c_str(), 1);
    tzset();
}

auto SolClock::ianaTimezoneString() -> std::string {
    return prefs_.tzIana.get().value_or("Etc/UTC");
}

auto SolClock::posixTimezoneString() -> std::string {
    return prefs_.tzPosix.get().value_or("UTC0");
}

void SolClock::setTimezone(const std::string &ianaTimezone) {
    if (ianaTimezone == prefs_.tzIana.get().value_or("Etc/UTC") &&
        prefs_.tzPosix.get().has_value()) {
        log_d("Timezone already set to %s", ianaTimezone.c_str());
        return;
    }

    prefs_.tzIana.set(ianaTimezone);
    std::string posixTimezoneString = ianaToPosix(ianaTimezone);
    prefs_.tzPosix.set(posixTimezoneString);

    log_d("Switching to timezone %s=%s", ianaTimezone.c_str(), posixTimezoneString.c_str());
    loadTimeZone();
}

auto SolClock::ianaToPosix(const std::string &ianaTimezone) -> std::string {
    SHOW_HEAP();
    std::istringstream ss(IANA_TO_POSIX_TIMEZONES);
    std::string line;
    SHOW_HEAP();

    while (std::getline(ss, line)) {
        std::string::size_type pos = line.find(ianaTimezone);
        if (pos != std::string::npos) {
            std::string::size_type commaPos = line.find(',', pos);
            if (commaPos != std::string::npos) {
                SHOW_HEAP();
                return line.substr(commaPos + 2); // skip comma and space
            }
        }
    }

    return "UTC0";
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

    // Set morning delivery to 6am and evening delivery to 8pm
    // if the user has toggled those options on.
    int morningNews = prefs_.noMorningDelivery.get().value_or(false) ? 0 : 6;
    int eveningNews = prefs_.eveningDelivery.get().value_or(false) ? 20 : 0;

    // If otherwise unselected, let's force a sync at 6am
    if (morningNews == 0 && eveningNews == 0) {
        morningNews = 6;
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
    if (eveningNews != 0 && eveningEventEpoch <= morningEventEpoch) {
        nextEventEpoch = eveningEventEpoch;
        size_t size = strftime(strftimeBuf, sizeof(strftimeBuf), "%c", &eveningEvent);
        if (size == 0) {
            log_e("strftime failed");
        }
    } else {
        nextEventEpoch = morningEventEpoch;
        size_t size = strftime(strftimeBuf, sizeof(strftimeBuf), "%c", &morningEvent);
        if (size == 0) {
            log_e("strftime failed");
        }
    }

    log_i("Next sync time is: %s (%" PRIu64 "s from now)", strftimeBuf,
          (int64_t)nextEventEpoch - now);

    return nextEventEpoch;
}

auto SolClock::getEpochTime() -> time_t { return time(nullptr); }
