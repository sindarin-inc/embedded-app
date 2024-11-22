#include "FirmwareDownload.hpp"

#include <Update.h>
#include <cstdio>
#include <string>
#include <utility>

#include "Communications/FileDownload.hpp"
#include "Communications/WiFi.hpp"
#include "Misc/SemVer.hpp"
#include "Misc/StringUtil.hpp"
#include "Version.hpp"
#include "sindarin-debug.h"

#if CONFIG_USE_BLE_SERVER_PROXY
#include "Communications/BLEManager.hpp"
#include "Communications/BLEProxy.hpp"
#endif
void FirmwareDownloader::downloadFirmware(
    const std::function<void(FirmwareDownloadState state)> &progressCallback) {

    auto setState = [progressCallback, this](FirmwareDownloadState state) {
        if (CancelFlag::isCancelled(cancel_)) {
            return;
        }
        progressCallback(std::move(state));
    };

    // Grab the token here from prefs if set
    auto token = prefs_.cloudToken.get();
    if (!token.has_value()) {
        log_w("No access token set for FirmwareDownloader");
    } else {
        SindarinAPI::getInstance()->setAccessToken(token.value());
    }

    setState(FirmwareDownloadState{.state = FirmwareDownloadState::State::CheckingForLatest});

    // Try WiFi first if not disabled
    bool wifiConnected = false;
    bool bleConnected = false;
    auto httpCode = 0;

    if (!prefs_.wifiDisabled.get().value_or(false)) {
        httpCode = SindarinAPI::getInstance()->checkFirmware(&response, false, cancel_);
        if (httpCode == 200) {
            wifiConnected = true;
        } else {
            log_e("Failed to get firmware over WiFi (HTTP %d)", httpCode);
#if !CONFIG_USE_BLE_SERVER_PROXY
            setState(FirmwareDownloadState{.state = FirmwareDownloadState::State::Failure,
                                           .message = "Failed to check for update"});
            return;
#endif
        }
    }

#if CONFIG_USE_BLE_SERVER_PROXY
    // Try BLE if WiFi failed and BLE is not disabled
    if (!wifiConnected && !prefs_.bleDisabled.get().value_or(false)) {
        if (!BLEManager::getInstance()->connectProxy(cancel_)) {
            setState(FirmwareDownloadState{.state = FirmwareDownloadState::State::Failure,
                                           .message = "Failed to connect via WiFi or BLE"});
            return;
        }
        bleConnected = true;

        // Disconnect WiFi if we're using BLE
        WiFi::getInstance().disconnectWiFi(3000);
        log_d("Checking for firmware update via BLE");
        httpCode = SindarinAPI::getInstance()->checkFirmware(&response, true, cancel_);
        if (httpCode != 200) {
            setState(FirmwareDownloadState{.state = FirmwareDownloadState::State::Failure,
                                           .message = "Failed to check for update via BLE"});
            return;
        }
    }
#endif

    if (!wifiConnected && !bleConnected) {
        setState(FirmwareDownloadState{.state = FirmwareDownloadState::State::Failure,
                                       .message = "No connection available"});
        return;
    }

    if (skipInstall_ &&
        prefs_.firmwareLoaded.get().value_or("0.0.0") == response.latestGlassesFirmware.version) {
        log_d("Firmware already downloaded and ready to install.");
        setState(FirmwareDownloadState{.state = FirmwareDownloadState::State::AlreadyDownloaded});
        return;
    }

    versionString = SemanticVersionString();
    log_d("Existing version: %s, server version: %s", versionString.c_str(),
          response.latestGlassesFirmware.version.c_str());
    if (SemVerCompareVersions(response.latestGlassesFirmware.version, versionString) !=
        SemVerComparison::GreaterThan) {
        setState(FirmwareDownloadState{.state = FirmwareDownloadState::State::AlreadyUpToDate});
        return;
    }

    versionString = response.latestGlassesFirmware.version;

    setState(
        FirmwareDownloadState{.state = FirmwareDownloadState::State::Downloading, .percentage = 0});

    log_d("Downloading firmware from %s", response.latestGlassesFirmware.url.c_str());
    std::string failMessage;
    httpCode = DownloadFileWithWriteBytes(
        response.latestGlassesFirmware.url.c_str(),
        [this, &failMessage](int httpCode, int contentLength) -> bool {
            if (httpCode != 200) {
                log_e("HTTP GET to %s failed, error: %d",
                      response.latestGlassesFirmware.url.c_str(), httpCode);
                failMessage = StringFormat("Download failed (%d)", httpCode);
                return false;
            }

            if (!Update.begin(contentLength, U_FLASH)) {
                auto error = Update.getError();
                log_e("Failed to Update.begin (err=%d contentLength=%d)", error, contentLength);
                failMessage = StringFormat("Failed to begin update (%d)", error);
                return false;
            }

            if (!Update.setMD5(response.latestGlassesFirmware.md5.c_str())) {
                auto error = Update.getError();
                log_e("Failed to Update.setMD5 (err=%d)", error);
                failMessage = StringFormat("Failed to begin update (%d)", error);
                return false;
            }

            return true;
        },
        [](const uint8_t *buf, size_t size) {
        return Update.write(const_cast<uint8_t *>(buf), size);
        },
        cancel_, bleConnected,
        [setState](int bytesWritten, float percentage) {
        setState(FirmwareDownloadState{.state = FirmwareDownloadState::State::Downloading,
                                       .percentage = static_cast<int>(percentage)});
    },
    false);

    if (httpCode != 200) {
        log_e("Failed to download firmware with code %d", httpCode);
        if (httpCode == ERR_CANCELLED) {
            setState(FirmwareDownloadState{.state = FirmwareDownloadState::State::Failure,
                                           .message = "Cancelled"});
            return;
        } else if (!WiFi::getInstance().connected()) {
            setState(FirmwareDownloadState{.state = FirmwareDownloadState::State::Failure,
                                           .message = "Failed: WiFi disconnected!"});
        } else {
            setState(FirmwareDownloadState{.state = FirmwareDownloadState::State::Failure,
                                           .message = failMessage});
        }
        Update.abort();
        return;
    }

    if (skipInstall_) {
        setState(FirmwareDownloadState{.state = FirmwareDownloadState::State::Downloaded});
        return;
    }

    setState(FirmwareDownloadState{.state = FirmwareDownloadState::State::Installing});

    // Swap out the firmware
    if (!Update.end()) {
        auto error = Update.getError();
        log_e("Failed to Update.end() (%d)", error);
        setState(FirmwareDownloadState{.state = FirmwareDownloadState::State::Failure,
                                       .message =
                                           StringFormat("Failed to finalize update (%d)", error)});
        return;
    }

    setState(FirmwareDownloadState{.state = FirmwareDownloadState::State::Installed});
}
