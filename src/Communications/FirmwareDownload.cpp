#include "FirmwareDownload.hpp"

#include <Update.h>
#include <cstdio>
#include <string>

#include "Communications/FileDownload.hpp"
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
        log_e("No access token set");
        progressCallback(FirmwareDownloadState{.state = FirmwareDownloadState::State::Failure,
                                               .message = "No access token set"});
        return;
    }
    SindarinAPI::getInstance()->setAccessToken(token.value());

    progressCallback(
        FirmwareDownloadState{.state = FirmwareDownloadState::State::CheckingForLatest});

    auto queue = DispatchGetCore0BackgroundQueue();
    queue->dispatch([this, progressCallback] {
        // Skip calculating the readable shas for now, the firmware info is sent either way
        CheckSyncResponse response;
        auto httpCode = SindarinAPI::getInstance()->checkSync("", &response);
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

        Update.setMD5(response.firmware.md5.c_str());
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
            log_e("[HTTP] GET... failed, error: %s\n", httpClient.errorToString(httpCode).c_str());
            progressCallback(
                FirmwareDownloadState{.state = FirmwareDownloadState::State::Failure,
                                      .message = StringFormat("Download failed (%d)", httpCode)});
            return;
        }

        auto success = DownloadFile(
            httpClient, 0,
            [&httpClient](const uint8_t *buf, size_t size) {
                if (!Update.isRunning()) {
                    // Get the size from the Content-Size header
                    if (!Update.begin(httpClient.getSize(), U_FLASH)) {
                        log_e("Failed to Update.begin (%d)", Update.getError());
                    }
                }
                return Update.write((uint8_t *)buf, size);
            },
            [progressCallback](int bytesWritten, float percentage) {
                progressCallback(
                    FirmwareDownloadState{.state = FirmwareDownloadState::State::Downloading,
                                          .downloadPercentage = (int)percentage});
            });

        if (!success) {
            log_e("Failed to download firmware");
            progressCallback(FirmwareDownloadState{.state = FirmwareDownloadState::State::Failure,
                                                   .message = "Failed to download udpate"});
            return;
        }

        progressCallback(FirmwareDownloadState{.state = FirmwareDownloadState::State::Downloaded,
                                               .downloadPercentage = 100});
    });
}

auto FirmwareDownloader::swapFirmware() -> bool { return Update.end(); }
