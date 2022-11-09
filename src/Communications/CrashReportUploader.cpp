#include "CrashReportUploader.hpp"

#include <cstdio>
#include <string>

#include "Communications/FileDownload.hpp"
#include "Communications/WiFi.hpp"
#include "GitVersion.hpp"
#include "Misc/Dispatch.hpp"
#include "Misc/StringUtil.hpp"
#include "Version.hpp"
#include "sindarin-debug.h"

#include <esp_core_dump.h>

void CrashUploader::uploadCrash(
    const std::function<void(CrashUploadState state)> &progressCallback) {

    /*
    // Grab the token here from prefs if set
    auto token = prefs_.cloudToken.get();
    if (!token.has_value()) {
        log_w("No access token set for FirmwareDownloader");
    } else {
        SindarinAPI::getInstance()->setAccessToken(token.value());
    }
    */

    // checking, can remove this
    //progressCallback(
    //    FirmwareDownloadState{.state = FirmwareDownloadState::State::CheckingForLatest});

    // begin by checking if crash report exists
    progressCallback(
        CrashUploadState{.state = CrashUploadState::State::CheckingForCrashReport});

    auto queue = DispatchGetCore0BackgroundQueue();
    queue->dispatch([this, progressCallback] {

        //versionString = response.firmware.version;

        // Now start downloading the firmware
        //progressCallback(FirmwareDownloadState{.state = FirmwareDownloadState::State::Downloading,
        //                                       .downloadPercentage = 0});

        // check for existence of core dump
        size_t out_addr, out_size;
        if (esp_core_dump_image_get(&out_addr, &out_size) != ESP_OK) {
            // error finding the core dump
            log_i("no crash report found.");
            progressCallback(CrashUploadState{.state = CrashUploadState::State::NoCrashReport});
            return;
        }

        log_i("about to upload the dump");
        std::string url = "https://sol-cloud.fly.dev/coredumps/";
        HTTPClient httpClient;
        if (!httpClient.begin(url.c_str())) {
            log_i("http failed for %s", url.c_str());
            progressCallback(CrashUploadState{.state = CrashUploadState::State::Failure,
            .message = "Failed to begin upload"});
            return;
        }
        
        log_i("begin succeeded");

        httpClient.addHeader("Content-Type", "multipart/form-data; boundary=5917222019993707403743335079");
        httpClient.setFollowRedirects(HTTPC_FORCE_FOLLOW_REDIRECTS);
        httpClient.setTimeout(30000);
        
        log_i("uploading firmware to %s", url.c_str());

        char test[4096];
        sprintf(test, "--5917222019993707403743335079--\r\nContent-Disposition: form-data; name=\"file\"; filename=\"testdump\"\r\nContent-Type: application/octet-stream\r\n\r\n12345689\r\n--5917222019993707403743335079--\r\n");
        
        log_i("sending: %s", test);
        int httpCode = httpClient.POST((uint8_t*)test, strlen(test));
        log_i("response: %d", httpCode);

        /*
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
        */


        //progressCallback(FirmwareDownloadState{.state = FirmwareDownloadState::State::Installed});
        progressCallback(CrashUploadState{.state = CrashUploadState::State::Done});
    });
}
