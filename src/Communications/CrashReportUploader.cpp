#include "CrashReportUploader.hpp"

#include <cstdio>
#include <esp_core_dump.h>
#include <esp_flash.h>
#include <string>

#include "Communications/FileDownload.hpp"
#include "Communications/MultiPartData.hpp"
#include "Communications/SindarinAPI.hpp"
#include "Communications/WiFi.hpp"
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

        FileList coreDumpFiles;
        fileSystem_.listDir("/coredumps", coreDumpFiles);

        if (coreDumpFiles.empty()) {
            log_i("no core dump found");
            progressCallback(CrashUploadState{.state = CrashUploadState::State::NoCrashReport});
            return;
        }

        int i = 0;
        for (auto &coreDumpFile : coreDumpFiles) {
            log_i("uploading %s", coreDumpFile.c_str());

            SFile cdFile = fileSystem_.open("/coredumps/" + coreDumpFile, FILE_READ);
            int httpCode = SindarinAPI::getInstance()->uploadCrashReport(
                cdFile, MultiPartData::getContentType());
            log_i("http response: %d", httpCode);
            cdFile.close();

            if (httpCode == 201) {
                // remove this core dump file
                log_i("deleting %s", coreDumpFile.c_str());
                fileSystem_.remove("/coredumps/" + coreDumpFile);
                progressCallback(CrashUploadState{
                    .state = CrashUploadState::State::Uploading,
                    .uploadPercentage = i * 100 / static_cast<int>(coreDumpFiles.size())});
            } else {
                log_e("could not send crash report %s, http code %d", coreDumpFile.c_str(),
                      httpCode);
                auto error = StringFormat("upload failed, code %d", httpCode);
                progressCallback(CrashUploadState{.state = CrashUploadState::State::Failure,
                                                  .message = std::move(error)});
                return;
            }
            i++;
        }

        progressCallback(CrashUploadState{.state = CrashUploadState::State::Done});
        return;
    });
}
