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

auto BackgroundSyncWithTransport(PowerManager &powerManager, PreferencesStore &prefs,
                                 LocalStore &localStore, Telemetry &telemetry, bool useWifi,
                                 bool useBle) -> bool {
    // Create a notification group to wait for background tasks
    log_d("Background sync starting with wifi: %d, ble: %d", useWifi, useBle);
    auto wifiConnected = false;
    auto bleConnected = false;

    auto backgroundGroup = xEventGroupCreate();
    defer {
        vEventGroupDelete(backgroundGroup);
        backgroundGroup = nullptr;
    };
    auto &fileSystem = localStore.fileSystem;

    cancel = std::make_shared<CancelFlag>();

    // Connect to WiFi
    if (useWifi) {
        if (!WiFi::getInstance().connectWiFiFromSettings()) {
            telemetry.logMetric(TelemetryField::METRIC_WIFI, "sync-wifi-failed");
            return false;
        } else {
            wifiConnected = true;
        }
    }

#if CONFIG_USE_BLE_SERVER_PROXY
    if (useBle && !wifiConnected) {
        if (!BLEManager::getInstance()->connectProxy(cancel)) {
            telemetry.logMetric(TelemetryField::METRIC_WIFI, "sync-ble-failed");
            return false;
        } else {
            bleConnected = true;
        }
    }
#endif

    // Check for firmware first (in case there's some super busted issue that needs fixing)
    if (wifiConnected) {
        auto firmwareDownloader = std::make_shared<FirmwareDownloader>(prefs, cancel, true);
        firmwareDownloader->downloadFirmware([firmwareDownloader, &localStore, &backgroundGroup](
                                                 const FirmwareDownloadState &state) {
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
            return false;
        }

        // Wait for firmware download to finish
        auto bits =
            xEventGroupWaitBits(backgroundGroup, 1, pdTRUE, pdTRUE, pdMS_TO_TICKS(10 * 60 * 1000));
        if (!(bits & 1)) {
            CancelFlag::cancel(cancel);
            log_w("Firmware download timed out");
        }
        xEventGroupClearBits(backgroundGroup, 1);
    }

    if (CancelFlag::isCancelled(cancel)) {
        return false;
    }

    auto syncSuccessful = false;

    localStore.syncFromRemote(wifiConnected, bleConnected, PreferencesStore::syncReason, cancel,
                              [&powerManager, &backgroundGroup, &telemetry,
                               &syncSuccessful](const ContentSyncState &state) {
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
                syncSuccessful = true;
            } else if (state.state == ContentSyncState::AlreadySynced) {
                telemetry.logMetric(TelemetryField::METRIC_SYNC_STATUS, "sync-already-synced");
                log_i("Content sync already synced");
                syncSuccessful = true;
            } else {
                return;
            }
            if (backgroundGroup != nullptr) {
                xEventGroupSetBits(backgroundGroup, 1);
            }
        }
    });
    log_d("Background content sync finished, syncSuccessful: %d", syncSuccessful);

    // Wait for content sync to finish
    auto bits =
        xEventGroupWaitBits(backgroundGroup, 1, pdTRUE, pdTRUE, pdMS_TO_TICKS(10 * 60 * 1000));
    if (!(bits & 1)) {
        CancelFlag::cancel(cancel);
        log_w("Content sync timed out");
    }
    xEventGroupClearBits(backgroundGroup, 1);

    if (CancelFlag::isCancelled(cancel)) {
        return false;
    }

    if (wifiConnected && !WiFi::getInstance().connected()) {
        log_w("WiFi not connected, ending background sync");
        return false;
    }
#if CONFIG_USE_BLE_SERVER_PROXY
    if (bleConnected && !BLEManager::getInstance()->connected()) {
        log_w("BLE proxy not connected, ending background sync");
        return false;
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
        return false;
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

            auto bits = xEventGroupWaitBits(backgroundGroup, 1, pdTRUE, pdTRUE,
                                            pdMS_TO_TICKS(2 * 60 * 1000));
            if (!(bits & 1)) {
                log_w("Coredump upload timed out");
            }
            xEventGroupClearBits(backgroundGroup, 1);
        }
    }

    return syncSuccessful;
}

void BackgroundSync(PowerManager &powerManager, PreferencesStore &prefs, LocalStore &localStore,
                    Telemetry &telemetry) {
    if (prefs.wifiDisabled.get().value_or(false) && prefs.bleDisabled.get().value_or(false)) {
        return;
    }

    // Debug LED so we know what's happening (will be turned off when deep sleep happens)
    // Only turn on if debug menu is enabled
    if (prefs.showDebugMenu.get().value_or(false)) {
        DigitalWrite(CONFIG_BLUE_LED, LED_ON);
    }

    // Try WiFi first if enabled
    auto syncSuccessful = false;
    if (!prefs.wifiDisabled.get().value_or(false)) {
        syncSuccessful =
            BackgroundSyncWithTransport(powerManager, prefs, localStore, telemetry, true, false);
    }

    // If WiFi sync failed and BLE is enabled, try BLE
    if (!syncSuccessful && !prefs.bleDisabled.get().value_or(false)) {
        // Disconnect from wifi if connected
        WiFi::getInstance().disconnectWiFi(3000);
        BackgroundSyncWithTransport(powerManager, prefs, localStore, telemetry, false, true);
    }

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
#if CONFIG_USE_BLE_SERVER_PROXY
    auto bleManager = BLEManager::getInstance(false);
    if (bleManager != nullptr) {
        bleManager->disconnectProxy();
    }
#endif

    PowerManager::getInstance().deepSleep(SleepReason::SyncDone);
}
