#include "FirmwareDownload.hpp"

#include <Update.h>
#include <cstdio>
#include <string>

#include "Communications/FileDownload.hpp"
#include "Communications/HttpClient.hpp"
#include "Communications/WiFi.hpp"
#include "GitVersion.hpp"
#include "Misc/Dispatch.hpp"
#include "Misc/StringUtil.hpp"
#include "Version.hpp"
#include "sindarin-debug.h"

void FirmwareDownloader::downloadFirmware(
    const std::function<void(FirmwareDownloadState state)> &progressCallback) {

    // Grab the token here from prefs if set
    auto token = prefs_.cloudToken.get();
    if (!token.has_value()) {
        log_w("No access token set for FirmwareDownloader");
    } else {
        SindarinAPI::getInstance()->setAccessToken(token.value());
    }

    progressCallback(
        FirmwareDownloadState{.state = FirmwareDownloadState::State::CheckingForLatest});

    auto queue = DispatchGetCore0BackgroundQueue();
    queue->dispatch([this, progressCallback] {
        // Skip calculating the readable shas and changed placemarks for now, the firmware info is
        // sent either way
        // TODO: we really need a way to get the latest firmware without calling /sync/all
        CheckSyncResponse response;
        CheckSyncRequest request = CheckSyncRequest{
            .readablesSince = "-1",
            .articlesSince = "-1",
            .readablePlacemarks = {},
            .articlePlacemarks = {},
            .ptBattery = prefs_.lastPageTurnerBattery.get().value_or(0),
            .ptBuild = prefs_.lastPageTurnerBuild.get().value_or(""),
            .skipContent = true,
        };
        auto httpCode = SindarinAPI::getInstance()->checkSync(request, &response);
        if (httpCode != 200) {
            log_e("Failed to check sync (HTTP %d)", httpCode);
            progressCallback(FirmwareDownloadState{.state = FirmwareDownloadState::State::Failure,
                                                   .message = "Failed to check for update"});
            return;
        }
        if (response.firmware.url.empty()) {
            versionString = kVersionString;
            progressCallback(
                FirmwareDownloadState{.state = FirmwareDownloadState::State::AlreadyUpToDate});
            return;
        }

        versionString = response.firmware.version;

        progressCallback(FirmwareDownloadState{.state = FirmwareDownloadState::State::Downloading,
                                               .downloadPercentage = 0});

        log_d("Downloading firmware from %s", response.firmware.url.c_str());
        auto success = DownloadFile(
            response.firmware.url.c_str(), 0,
            [&response, &progressCallback](int httpCode, int contentLength) -> bool {
                HttpClient client(response.firmware.url.c_str());
                if (httpCode != 200) {
                    log_e("HTTP GET failed, error: %d\n", httpCode);
                    progressCallback(FirmwareDownloadState{
                        .state = FirmwareDownloadState::State::Failure,
                        .message = StringFormat("Download failed (%d)", httpCode)});
                    return false;
                }

                if (!Update.begin(contentLength, U_FLASH)) {
                    auto error = Update.getError();
                    log_e("Failed to Update.begin (err=%d contentLength=%d)", error, contentLength);
                    progressCallback(FirmwareDownloadState{
                        .state = FirmwareDownloadState::State::Failure,
                        .message = StringFormat("Failed to begin update (%d)", error)});
                    return false;
                }

                Update.setMD5(response.firmware.md5.c_str());

                return true;
            },
            [](const uint8_t *buf, size_t size) { return Update.write((uint8_t *)buf, size); },
            [progressCallback](int bytesWritten, float percentage) {
                progressCallback(
                    FirmwareDownloadState{.state = FirmwareDownloadState::State::Downloading,
                                          .downloadPercentage = static_cast<int>(percentage)});
            });

        if (!success) {
            log_e("Failed to download firmware");
            if (!NetworkConnected()) {
                progressCallback(
                    FirmwareDownloadState{.state = FirmwareDownloadState::State::Failure,
                                          .message = "Failed: WiFi disconnected!"});
            } else {
                progressCallback(
                    FirmwareDownloadState{.state = FirmwareDownloadState::State::Failure,
                                          .message = "Failed to download update"});
            }
            Update.abort();
            return;
        }

        progressCallback(FirmwareDownloadState{.state = FirmwareDownloadState::State::Installing});

        // Swap out the firmware
        if (!Update.end()) {
            auto error = Update.getError();
            log_e("Failed to Update.end() (%d)", error);
            progressCallback(FirmwareDownloadState{
                .state = FirmwareDownloadState::State::Failure,
                .message = StringFormat("Failed to finalize update (%d)", error)});
            return;
        }

        progressCallback(FirmwareDownloadState{.state = FirmwareDownloadState::State::Installed});
    });
}
