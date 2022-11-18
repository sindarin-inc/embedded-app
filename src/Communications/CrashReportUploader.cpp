#include "CrashReportUploader.hpp"

#include <cstdio>
#include <esp_core_dump.h>
#include <esp_flash.h>
#include <string>

#include "Communications/FileDownload.hpp"
#include "Communications/MultiPartData.hpp"
#include "Communications/SindarinAPI.hpp"
#include "Communications/WiFi.hpp"
#include "GitVersion.hpp"
#include "Misc/Dispatch.hpp"
#include "Misc/Platform.hpp"
#include "Misc/StringUtil.hpp"
#include "Version.hpp"
#include "sindarin-debug.h"

void CrashUploader::uploadCrash(
    const std::function<void(CrashUploadState state)> &progressCallback) {

    // begin by checking if crash report exists
    progressCallback(CrashUploadState{.state = CrashUploadState::State::CheckingForCrashReport});

    auto queue = DispatchGetCore0BackgroundQueue();
    queue->dispatch([this, progressCallback] {
        // Grab the token here from prefs if set
        auto token = prefs_.cloudToken.get();
        if (!token.has_value()) {
            log_w("No access token set for CrashUploader");
        } else {
            SindarinAPI::getInstance()->setAccessToken(token.value());
        }

        // check for existence of core dump
        log_i("checking core dump");
        size_t outAddr, outSize;
        if (esp_core_dump_image_get(&outAddr, &outSize) != ESP_OK) {
            // error finding the core dump
            log_i("no crash report found.");
            progressCallback(CrashUploadState{.state = CrashUploadState::State::NoCrashReport});
            return;
        }

        log_i("about to upload the dump");
        MultiPartData mpData(fileSystem_);
        mpData.startFile("file");

        uint8_t chunkBuffer[1024];
        int bytesRemaining = outSize;
        auto currAddr = (uint32_t)outAddr;
        while (bytesRemaining > 0) {
            // read chunk
            int bytesToRead = bytesRemaining >= 1024 ? 1024 : bytesRemaining;
            if (esp_flash_read(nullptr, chunkBuffer, currAddr, bytesToRead) != ESP_OK) {
                log_e("could not read crash report from flash");
                progressCallback(CrashUploadState{.state = CrashUploadState::State::Failure});
                return;
            }

            mpData.writeFileChunk(chunkBuffer, bytesToRead);

            // updates
            bytesRemaining -= bytesToRead;
            currAddr += bytesToRead;
        }
        mpData.endFile();
        mpData.addKeyValue("git_version", kGitHash);
        mpData.addKeyValue("serial_number", GetSerialNumber());
        mpData.addKeyValue("fw_version", kVersionString);
        mpData.endMultiPartData();

        int httpCode = SindarinAPI::getInstance()->uploadCrashReport(mpData.getFile(),
                                                                     mpData.getContentType());
        log_i("http response: %d", httpCode);

        if (httpCode == 201) {
            progressCallback(CrashUploadState{.state = CrashUploadState::State::Done});
            return;
        }

        log_e("could not send crash report, http code %d", httpCode);
        char errMsg[64];
        snprintf(errMsg, sizeof(errMsg), "upload failed, code %d", httpCode);
        progressCallback(
            CrashUploadState{.state = CrashUploadState::State::Failure, .message = errMsg});
    });
}
