#include "FirmwareDownload.hpp"

#include <Update.h>
#include <cstdio>
#include <string>
#include <utility>

#include "Communications/FileDownload.hpp"
#include "Communications/WiFi.hpp"
#include "GitVersion.hpp"
#include "Misc/Dispatch.hpp"
#include "Misc/SemVer.hpp"
#include "Misc/StringUtil.hpp"
#include "Version.hpp"
#include "sindarin-debug.h"

void FirmwareDownloader::downloadFirmware(
    const std::function<void(FirmwareDownloadState state)> &progressCallback) {

    auto setState = [progressCallback, this](FirmwareDownloadState state) {
        // Don't call the callback if we've been cancelled
        if (canelled_) {
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

    auto queue = DispatchGetCore0BackgroundQueue();
    queue->dispatch([this, setState] {
        auto httpCode = SindarinAPI::getInstance()->checkFirmware(&response);
        if (httpCode != 200) {
            log_e("Failed to get firmware (HTTP %d)", httpCode);
            setState(FirmwareDownloadState{.state = FirmwareDownloadState::State::Failure,
                                           .message = "Failed to check for update"});
            return;
        }

        if (skipInstall_ && prefs_.firmwareLoaded.get().value_or("0.0.0") ==
                                response.latestGlassesFirmware.version) {
            log_d("Firmware already downloaded and ready to install.");
            setState(
                FirmwareDownloadState{.state = FirmwareDownloadState::State::AlreadyDownloaded});
            return;
        }

        versionString = kVersionString;
        log_d("Existing version: %s, server version: %s", versionString.c_str(),
              response.latestGlassesFirmware.version.c_str());
        if (SemVerCompareVersions(response.latestGlassesFirmware.version, versionString) !=
            SemVerComparison::GreaterThan) {
            setState(FirmwareDownloadState{.state = FirmwareDownloadState::State::AlreadyUpToDate});
            return;
        }

        versionString = response.latestGlassesFirmware.version;

        setState(FirmwareDownloadState{.state = FirmwareDownloadState::State::Downloading,
                                       .downloadPercentage = 0});

        log_d("Downloading firmware from %s", response.latestGlassesFirmware.url.c_str());
        std::string failMessage = "";
        auto success = DownloadFile(
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
            [](const uint8_t *buf, size_t size) { return Update.write((uint8_t *)buf, size); },
            [setState](int bytesWritten, float percentage) {
            setState(FirmwareDownloadState{.state = FirmwareDownloadState::State::Downloading,
                                           .downloadPercentage = static_cast<int>(percentage)});
            },
            false);

        if (!success) {
            log_e("Failed to download firmware");
            if (!WiFi::getInstance().connected()) {
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
            setState(FirmwareDownloadState{
                .state = FirmwareDownloadState::State::Failure,
                .message = StringFormat("Failed to finalize update (%d)", error)});
            return;
        }

        setState(FirmwareDownloadState{.state = FirmwareDownloadState::State::Installed});
    });
}
