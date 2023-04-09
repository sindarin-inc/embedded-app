#include "BackgroundSync.hpp"

#include <freertos/FreeRTOS.h>

#include "Communications/CrashReportUploader.hpp"
#include "Communications/FirmwareDownload.hpp"
#include "Communications/SindarinAPI.hpp"
#include "Communications/WiFi.hpp"
#include "Misc/Platform.hpp"
#include "UI/ContentSyncScreen.hpp"
#include "config.h"

void BackgroundSync(PowerManager &powerManager, PreferencesStore &prefs, LocalStore &localStore) {
    // Debug LED so we know what's happening (will be turned off when deep sleep happens)
    DigitalWrite(BLUE_LED, LED_ON);

    // Create a notification group to wait for background tasks
    auto backgroundGroup = xEventGroupCreate();
    auto &fileSystem = localStore.fileSystem;

    localStore.syncFromRemote([&powerManager, &backgroundGroup](const ContentSyncState &state) {
        powerManager.feedInactivityTimer();

        if (state.state == ContentSyncState::Failure || state.state == ContentSyncState::Synced ||
            state.state == ContentSyncState::AlreadySynced) {
            if (state.state == ContentSyncState::Failure) {
                log_i("Content sync failed with error: %s", state.message.c_str());
            } else if (state.state == ContentSyncState::Synced) {
                log_i("Content sync successful");
            } else if (state.state == ContentSyncState::AlreadySynced) {
                log_i("Content sync already synced");
            } else {
                return;
            }
            xEventGroupSetBits(backgroundGroup, 1);
        }
    });

    // Wait for content sync to finish
    auto bits =
        xEventGroupWaitBits(backgroundGroup, 1, pdTRUE, pdTRUE, pdMS_TO_TICKS(5 * 60 * 1000));
    if (!(bits & 1)) {
        log_w("Content sync timed out");
    }

    if (!WiFi::getInstance().connected()) {
        log_w("WiFi not connected, ending background sync");
        return;
    }

    SolClock::getInstance().setClockTimeFromInternet();

    FirmwareDownloader firmwareDownloader(prefs, true);
    firmwareDownloader.downloadFirmware(
        [&firmwareDownloader, &localStore, &backgroundGroup](const FirmwareDownloadState &state) {
            if (state.state == FirmwareDownloadState::Failure) {
                log_i("Firmware download for %s failed with error: %s",
                      firmwareDownloader.response.latestGlassesFirmware.version.c_str(),
                      state.message.c_str());
            } else if (state.state == FirmwareDownloadState::AlreadyDownloaded) {
                log_i("Firmware download for %s already queued.",
                      firmwareDownloader.response.latestGlassesFirmware.version.c_str());
            } else if (state.state == FirmwareDownloadState::Downloaded) {
                log_i("Firmware download for %s successful. Mark for install.",
                      firmwareDownloader.response.latestGlassesFirmware.version.c_str());
                localStore.saveFirmware(firmwareDownloader.response.latestGlassesFirmware);
            } else if (state.state == FirmwareDownloadState::AlreadyUpToDate) {
                log_i("Firmware up to date.");
            } else {
                return;
            }
            xEventGroupSetBits(backgroundGroup, 1);
        });

    // Wait for firmware download to finish
    bits = xEventGroupWaitBits(backgroundGroup, 1, pdTRUE, pdTRUE, pdMS_TO_TICKS(5 * 60 * 1000));
    if (!(bits & 1)) {
        log_w("Firmware download timed out");
    }

    // Upload core dumps
    FileList coreDumpFiles;
    const int maxCoredumps = 10;
    fileSystem.listDir("/coredumps", coreDumpFiles, maxCoredumps);
    int coreDumpCount = coreDumpFiles.size();

    if (!(prefs.noUploadCrashes.get().value_or(false)) && (coreDumpCount > 0)) {
        log_i("Uploading %d core dumps", coreDumpCount);
        CrashUploader crashUploader(fileSystem, prefs);
        crashUploader.uploadCrash([&backgroundGroup](const CrashUploadState &state) {
            if (state.state == CrashUploadState::Failure) {
                log_e("Failed to upload crash reports");
            } else if (state.state == CrashUploadState::NoCrashReport) {
                log_w("No crash reports (shouldn't get here)");
            } else if (state.state == CrashUploadState::Done) {
                log_i("Crash reports uploaded");
            } else {
                return;
            }
            xEventGroupSetBits(backgroundGroup, 1);
        });

        auto bits =
            xEventGroupWaitBits(backgroundGroup, 1, pdTRUE, pdTRUE, pdMS_TO_TICKS(2 * 60 * 1000));
        if (!(bits & 1)) {
            log_w("Coredump upload timed out");
        }
    }
}

// TODO: It would probably be good to do some sort of retry with exponential
// backup if we fail. But We don't want to drain battery if you're away from
// WiFi. So we'll need to think carefully how we want to handle that.
void BackgroundSyncAndSleep(PowerManager &powerManager, PreferencesStore &prefs,
                            LocalStore &localStore) {

    BackgroundSync(powerManager, prefs, localStore);

    // TODO: Perhaps try dispatching this (maybe needs to happen outside of app_main)
    WiFi::getInstance().disconnectWiFi();

    // Figure out the next content sync and set the best time for it
    time_t now = time(nullptr);
    PreferencesStore::syncReason = SyncReason::Periodic;
    PreferencesStore::nextSyncTime = SolClock::getInstance().calculateNextSyncTime(now);
    PowerManager::getInstance().ulpDeepSleep(SleepReason::SyncDone);
}
