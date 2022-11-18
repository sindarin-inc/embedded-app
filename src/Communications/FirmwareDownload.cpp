#include "FirmwareDownload.hpp"

#include <Update.h>
#include <cstdio>
#include <string>

#include "Communications/FileDownload.hpp"
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
        CheckSyncResponse response;
        auto httpCode = SindarinAPI::getInstance()->checkSync("", {}, &response);
        if (httpCode != HTTP_CODE_OK) {
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

        // Now start downloading the firmware
        progressCallback(FirmwareDownloadState{.state = FirmwareDownloadState::State::Downloading,
                                               .downloadPercentage = 0});

        // TODO(john): Would probably be good to centralize our HTTPClient creation
        HTTPClient httpClient;
        if (!httpClient.begin(response.firmware.url.c_str())) {
            log_e("HTTPClient::begin failed for URL %s", response.firmware.url.c_str());
            progressCallback(FirmwareDownloadState{.state = FirmwareDownloadState::State::Failure,
                                                   .message = "Failed to begin download"});
            return;
        }

        httpClient.setUserAgent("Sindarin/" + String(kVersionString) + " (" + String(kGitHash) +
                                ")");
        httpClient.setFollowRedirects(HTTPC_FORCE_FOLLOW_REDIRECTS);
        httpClient.setTimeout(30000);

        log_d("Downloading firmware from %s", response.firmware.url.c_str());
        httpCode = httpClient.GET();
        // HTTP header has been sent and Server response header has been handled
        // file found at server
        if (httpCode != HTTP_CODE_OK) {
            log_e("HTTP GET failed, error: %s\n", httpClient.errorToString(httpCode).c_str());
            httpClient.end();
            progressCallback(
                FirmwareDownloadState{.state = FirmwareDownloadState::State::Failure,
                                      .message = StringFormat("Download failed (%d)", httpCode)});
            return;
        }

        auto length = httpClient.getSize();

        if (!Update.begin(length, U_FLASH)) {
            auto error = Update.getError();
            log_e("Failed to Update.begin (err=%d length=%d)", error, length);
            httpClient.end();
            progressCallback(FirmwareDownloadState{
                .state = FirmwareDownloadState::State::Failure,
                .message = StringFormat("Failed to begin update (%d)", error)});
            return;
        }
        Update.setMD5(response.firmware.md5.c_str());

        auto success = DownloadFile(
            httpClient, length,
            [](const uint8_t *buf, size_t size) { return Update.write((uint8_t *)buf, size); },
            [progressCallback](int bytesWritten, float percentage) {
                progressCallback(
                    FirmwareDownloadState{.state = FirmwareDownloadState::State::Downloading,
                                          .downloadPercentage = (int)percentage});
            });

        httpClient.end();

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
