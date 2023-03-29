#include "SolClock.hpp"

#if !SIMULATOR
#include <esp_log.h>
#include <esp_sntp.h>
#endif

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

void SolClock::getClockTime(uint8_t &hours, uint8_t &minutes) {
#if SIMULATOR
    if (!useSimulatorTestTime) {
        time_t now = time(0);
        tm *local = localtime(&now);
        hours = local->tm_hour;
        minutes = local->tm_min;
    } else {
        hours = simulatorTestHours;
        minutes = simulatorTestMinutes;
    }
#else
    time_t now;
    struct tm timeinfo;
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
    time(&now);

    setenv("TZ", timeZoneString.c_str(), 1);
    tzset();

    localtime_r(&now, &timeinfo);

    hours = timeinfo.tm_hour;
    minutes = timeinfo.tm_min;
#endif
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
        time(&now);
        localtime_r(&now, &timeinfo);
    }

#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 1, 0)
    esp_sntp_stop();
#else
    sntp_stop();
#endif

    // Print the time as a string
    char strftimeBuf[64];
    strftime(strftimeBuf, sizeof(strftimeBuf), "%c", &timeinfo);
    log_i("The current date/time is: %s", strftimeBuf);
#else
    log_i("Not setting clock time from internet in simulator");
#endif
}

auto SolClock::getEpochTime() -> time_t { return time(nullptr); }