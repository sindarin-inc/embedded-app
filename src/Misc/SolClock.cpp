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
    if (!ConnectWifi()) {
        log_i("Failed to connect wifi");
        return;
    }
    // Configure the SNTP client
    sntp_setoperatingmode(SNTP_OPMODE_POLL);
    sntp_setservername(0, "time.google.com");
    sntp_init();

    // Wait for the SNTP client to get the time
    time_t now = 0;
    struct tm timeinfo {};
    while (timeinfo.tm_year < (2021 - 1900)) {
        log_i("Waiting for system time to be set...");
        vTaskDelay(pdMS_TO_TICKS(1000));
        time(&now);
        localtime_r(&now, &timeinfo);
    }

    // Print the time as a string
    char strftimeBuf[64];
    strftime(strftimeBuf, sizeof(strftimeBuf), "%c", &timeinfo);
    log_i("The current date/time is: %s", strftimeBuf);

    DisconnectWifi();
#else
    log_i("Not setting clock time from internet in simulator");
#endif
}
