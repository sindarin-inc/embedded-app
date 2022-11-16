#include "CrashReportUploader.hpp"

#include <cstdio>
#include <string>

#include "Communications/FileDownload.hpp"
#include "Communications/WiFi.hpp"
#include "Communications/MultiPartData.hpp"
#include "GitVersion.hpp"
#include "Misc/Dispatch.hpp"
#include "Misc/StringUtil.hpp"
#include "Version.hpp"
#include "sindarin-debug.h"
#include "Misc/Platform.hpp"

#include <esp_core_dump.h>
#include <esp_flash.h>

void CrashUploader::uploadCrash(
    const std::function<void(CrashUploadState state)> &progressCallback) {

    // begin by checking if crash report exists
    progressCallback(
        CrashUploadState{.state = CrashUploadState::State::CheckingForCrashReport});

    auto queue = DispatchGetCore0BackgroundQueue();
    queue->dispatch([this, progressCallback] {

        // check for existence of core dump
        log_i("checking core dump");
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

        MultiPartData mpData(fileSystem_);
        mpData.startFile("file");

        uint8_t chunkBuffer[1024];
        int bytesRemaining = out_size;
        uint32_t currAddr = (uint32_t)out_addr;
        while (bytesRemaining > 0) {
            // read chunk
            int bytesToRead = bytesRemaining >= 1024 ? 1024 : bytesRemaining;
            if (esp_flash_read(NULL, chunkBuffer, currAddr, bytesToRead) != ESP_OK) {
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
        // todo: get real string for hw version
        //mpData.addKeyValue("hw_version", "something");

        int httpCode = mpData.sendPost(&httpClient);
        log_i("http response: %d", httpCode);
        httpClient.end();

        if (httpCode == 201) {
            progressCallback(CrashUploadState{.state = CrashUploadState::State::Done});
            return;
        }

        log_e("could not send crash report, http code %d", httpCode);
        progressCallback(CrashUploadState{.state = CrashUploadState::State::Failure, .message = "upload failed, code " + httpCode});
    });
}
