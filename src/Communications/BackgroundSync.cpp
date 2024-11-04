#include "BackgroundSync.hpp"

#include <freertos/FreeRTOS.h>

#if CONFIG_USE_BLE_SERVER_PROXY || USE_BLE_SERVER_REMOTE
#include "Communications/BLEManager.hpp"
#endif
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

void BackgroundSync(PowerManager &powerManager, PreferencesStore &prefs, LocalStore &localStore,
                    Telemetry &telemetry, FileSystem &fileSystem) {

    // Don't perform a background sync if wifi is disabled manually
    // TODO: Once BLE sync is merged, check to see if that is still enabled
    // and if so, continue with the sync but DON'T use wifi for it. Maybe have to
    // change the guts of this a bit to pull that off.
    if (prefs.wifiDisabled.get().value_or(false) && prefs.bleDisabled.get().value_or(false)) {
        return;
    }

    auto wifiConnected = false;
#if CONFIG_USE_BLE_SERVER_PROXY
    auto bleConnected = false;
#endif

    cancel = std::make_shared<CancelFlag>();

    // Connect to WiFi
    if (!WiFi::getInstance().connectWiFiFromSettings()) {
        telemetry.logMetric(TelemetryField::METRIC_WIFI, "sync-wifi-failed");
    } else {
        wifiConnected = true;
    }

#if CONFIG_USE_BLE_SERVER_PROXY
    if (!wifiConnected) {
        if (!BLEManager::getInstance()->connectProxy(cancel)) {
            telemetry.logMetric(TelemetryField::METRIC_WIFI, "sync-ble-failed");
            return;
        } else {
            bleConnected = true;
        }
    }
#endif

    // Check for firmware first (in case there's some super busted issue that needs fixing)
    if (wifiConnected) {
        auto firmwareDownloader = std::make_shared<FirmwareDownloader>(prefs, cancel, true);
        firmwareDownloader->downloadFirmware(
            [firmwareDownloader, &localStore](const FirmwareDownloadState &state) {
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
        });
    }

    if (CancelFlag::isCancelled(cancel)) {
        return;
    }

    localStore.syncFromRemote(PreferencesStore::syncReason, cancel,
                              [&powerManager, &telemetry](const ContentSyncState &state) {
        powerManager.feedInactivityTimer();

        if (state.state == ContentSyncState::Failure || state.state == ContentSyncState::Synced ||
            state.state == ContentSyncState::AlreadySynced) {
            if (state.state == ContentSyncState::Failure) {
                telemetry.logMetric(TelemetryField::METRIC_SYNC_STATUS, "sync-failed",
                                    {{"message", state.message}});
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
        }
    });

    if (CancelFlag::isCancelled(cancel)) {
        return;
    }

    if (wifiConnected && !WiFi::getInstance().connected()) {
        log_w("WiFi not connected, ending background sync");
        return;
    }
#if CONFIG_USE_BLE_SERVER_PROXY
    if (bleConnected && !BLEManager::getInstance()->connected()) {
        log_w("BLE proxy not connected, ending background sync");
        return;
    }
#endif

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
    if (wifiConnected) {
        FileList coreDumpFiles;
        const int maxCoredumps = 10;
        fileSystem.listDir("/coredumps", coreDumpFiles, maxCoredumps);
        int coreDumpCount = coreDumpFiles.size();

        if (coreDumpCount > 0) {
            log_i("Uploading %d core dumps", coreDumpCount);
            CrashUploader crashUploader(fileSystem, prefs);
            crashUploader.uploadCrash([](const CrashUploadState &state) {
                if (state.state == CrashUploadState::Failure) {
                    log_e("Failed to upload crash reports");
                } else if (state.state == CrashUploadState::NoCrashReport) {
                    log_w("No crash reports (shouldn't get here)");
                } else if (state.state == CrashUploadState::Done) {
                    log_i("Crash reports uploaded");
                } else {
                    return;
                }
            });
        }
    }
}

// TODO: It would probably be good to do some sort of retry with exponential
// backup if we fail. But We don't want to drain battery if you're away from
// WiFi. So we'll need to think carefully how we want to handle that.
void BackgroundSyncAndSleep(PowerManager &powerManager, PreferencesStore &prefs,
                            LocalStore &localStore, Telemetry &telemetry, FileSystem &fileSystem) {
    // If debug menu is enabled, light up the blue LED so we know what's happening
    if (prefs.showDebugMenu.get().value_or(false)) {
        DigitalWrite(CONFIG_BLUE_LED, LED_ON);
    }

    DispatchQueue::dispatchOneOffSync(
        [&powerManager, &prefs, &localStore, &telemetry, &fileSystem]() {
        BackgroundSync(powerManager, prefs, localStore, telemetry, fileSystem);
        },
        8192, 1, 0, "Sync", pdMS_TO_TICKS(15 * 60 * 1000));

    BackgroundSyncCancel();

    WiFi::getInstance().disconnectWiFi(3000);
#if CONFIG_USE_BLE_SERVER_PROXY
    auto bleManager = BLEManager::getInstance(false);
    if (bleManager != nullptr) {
        bleManager->disconnectProxy();
    }
#endif

    if (prefs.showDebugMenu.get().value_or(false)) {
        DigitalWrite(CONFIG_BLUE_LED, LED_OFF);
    }

    PowerManager::getInstance().deepSleep(SleepReason::SyncDone);
}
