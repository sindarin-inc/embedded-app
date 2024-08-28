#include "BackgroundSync.hpp"

#include <freertos/FreeRTOS.h>

#include "Communications/CrashReportUploader.hpp"
#include "Communications/FirmwareDownload.hpp"
#include "Communications/SindarinAPI.hpp"
#include "Communications/WiFi.hpp"
#include "Misc/CancelFlag.hpp"
#include "Misc/Platform.hpp"
#include "Misc/Telemetry.hpp"
#include "config.h"

std::shared_ptr<CancelFlag> cancel;

void BackgroundSyncCancel() { CancelFlag::cancel(cancel); }

void BackgroundSyncInternal(PowerManager &powerManager, PreferencesStore &prefs,
                            LocalStore &localStore, Telemetry &telemetry) {
    // Create a notification group to wait for background tasks
    auto backgroundGroup = xEventGroupCreate();
    defer {
        vEventGroupDelete(backgroundGroup);
        backgroundGroup = nullptr;
    };
    auto &fileSystem = localStore.fileSystem;

    cancel = std::make_shared<CancelFlag>();

    // Connect to WiFi
    if (!WiFi::getInstance().connectWiFiFromSettings()) {
        telemetry.logMetric(TelemetryField::METRIC_WIFI, "sync-wifi-failed");
        return;
    }

    // Check for firmware first (in case there's some super busted issue that needs fixing)
    auto firmwareDownloader = std::make_shared<FirmwareDownloader>(prefs, true);
    firmwareDownloader->downloadFirmware(
        [firmwareDownloader, &localStore, &backgroundGroup](const FirmwareDownloadState &state) {
        if (state.state == FirmwareDownloadState::Failure) {
            log_i("Firmware download for %s failed with error: %s",
                  firmwareDownloader->response.latestGlassesFirmware.version.c_str(),
                  state.message.c_str());
        } else if (state.state == FirmwareDownloadState::AlreadyDownloaded) {
            log_i("Firmware download for %s already queued.",
                  firmwareDownloader->response.latestGlassesFirmware.version.c_str());
        } else if (state.state == FirmwareDownloadState::Downloaded) {
            log_i("Firmware download for %s successful. Mark for install.",
                  firmwareDownloader->response.latestGlassesFirmware.version.c_str());
            localStore.saveFirmware(firmwareDownloader->response.latestGlassesFirmware);
        } else if (state.state == FirmwareDownloadState::AlreadyUpToDate) {
            log_i("Firmware up to date.");
        } else {
            return;
        }
        if (backgroundGroup != nullptr) {
            xEventGroupSetBits(backgroundGroup, 1);
        }
    });

    if (CancelFlag::isCancelled(cancel)) {
        return;
    }

    // Wait for firmware download to finish
    auto bits =
        xEventGroupWaitBits(backgroundGroup, 1, pdTRUE, pdTRUE, pdMS_TO_TICKS(5 * 60 * 1000));
    if (!(bits & 1)) {
        firmwareDownloader->cancel();
        log_w("Firmware download timed out");
    }
    xEventGroupClearBits(backgroundGroup, 1);

    if (CancelFlag::isCancelled(cancel)) {
        return;
    }

    localStore.syncFromRemote(
        PreferencesStore::syncReason, cancel,
        [&powerManager, &backgroundGroup, &telemetry](const ContentSyncState &state) {
        powerManager.feedInactivityTimer();

        if (state.state == ContentSyncState::Failure || state.state == ContentSyncState::Synced ||
            state.state == ContentSyncState::AlreadySynced) {
            if (state.state == ContentSyncState::Failure) {
                telemetry.logMetric(TelemetryField::METRIC_SYNC_STATUS, "sync-failed",
                                    {{"message", state.message.c_str()}});
                log_i("Content sync failed with error: %s", state.message.c_str());
            } else if (state.state == ContentSyncState::Synced) {
                telemetry.logMetric(TelemetryField::METRIC_SYNC_STATUS, "sync-success");
                log_i("Content sync successful");
            } else if (state.state == ContentSyncState::AlreadySynced) {
                telemetry.logMetric(TelemetryField::METRIC_SYNC_STATUS, "sync-already-synced");
                log_i("Content sync already synced");
            } else {
                return;
            }
            if (backgroundGroup != nullptr) {
                xEventGroupSetBits(backgroundGroup, 1);
            }
        }
        });

    // Wait for content sync to finish
    bits = xEventGroupWaitBits(backgroundGroup, 1, pdTRUE, pdTRUE, pdMS_TO_TICKS(10 * 60 * 1000));
    if (!(bits & 1)) {
        CancelFlag::cancel(cancel);
        log_w("Content sync timed out");
    }
    xEventGroupClearBits(backgroundGroup, 1);

    if (CancelFlag::isCancelled(cancel)) {
        return;
    }

    if (!WiFi::getInstance().connected()) {
        log_w("WiFi not connected, ending background sync");
        return;
    }

    telemetry.logMetric(TelemetryField::METRIC_CLOCK_SET, "pre-sync",
                        {{"run-time", GetRunTimeMs()}});
    auto success = SolClock::getInstance().setClockTimeFromInternet();
    if (success) {
        telemetry.logMetric(TelemetryField::METRIC_CLOCK_SET, "success",
                            {{"run-time", GetRunTimeMs()}});
    } else {
        telemetry.logMetric(TelemetryField::METRIC_CLOCK_SET, "failure",
                            {{"run-time", GetRunTimeMs()}});
    }

    if (CancelFlag::isCancelled(cancel)) {
        return;
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
            if (backgroundGroup != nullptr) {
                xEventGroupSetBits(backgroundGroup, 1);
            }
        });

        bits =
            xEventGroupWaitBits(backgroundGroup, 1, pdTRUE, pdTRUE, pdMS_TO_TICKS(2 * 60 * 1000));
        if (!(bits & 1)) {
            log_w("Coredump upload timed out");
        }
        xEventGroupClearBits(backgroundGroup, 1);
    }
}

void BackgroundSync(PowerManager &powerManager, PreferencesStore &prefs, LocalStore &localStore,
                    Telemetry &telemetry) {
    // Debug LED so we know what's happening (will be turned off when deep sleep happens)
    // Only turn on if debug menu is enabled
    if (prefs.showDebugMenu.get().value_or(false)) {
        DigitalWrite(CONFIG_BLUE_LED, LED_ON);
    }

    BackgroundSyncInternal(powerManager, prefs, localStore, telemetry);

    if (prefs.showDebugMenu.get().value_or(false)) {
        DigitalWrite(CONFIG_BLUE_LED, LED_OFF);
    }
}

// TODO: It would probably be good to do some sort of retry with exponential
// backup if we fail. But We don't want to drain battery if you're away from
// WiFi. So we'll need to think carefully how we want to handle that.
void BackgroundSyncAndSleep(PowerManager &powerManager, PreferencesStore &prefs,
                            LocalStore &localStore, Telemetry &telemetry) {

    BackgroundSync(powerManager, prefs, localStore, telemetry);

    // TODO: Perhaps try dispatching this (maybe needs to happen outside of app_main)
    WiFi::getInstance().disconnectWiFi(3000);

    PowerManager::getInstance().deepSleep(SleepReason::SyncDone);
}
