#include "PreferencesStore.hpp"

#include <esp_attr.h>

RTC_SLOW_ATTR SyncReason PreferencesStore::syncReason;
RTC_SLOW_ATTR uint64_t PreferencesStore::nextSyncTime;
RTC_SLOW_ATTR uint8_t PreferencesStore::lastBatteryReport;
RTC_SLOW_ATTR uint8_t PreferencesStore::lastPageTurnerBattery;
RTC_SLOW_ATTR bool PreferencesStore::settingsOnRestart;
