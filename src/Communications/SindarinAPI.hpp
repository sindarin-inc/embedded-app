#pragma once
#include <ArduinoJson.h>
#include <esp_http_client.h>
#include <functional>

#include "Communications/HttpClient.hpp"
#include "Models/CheckSync.hpp"
#include "Models/Firmware.hpp"
#include "Models/FirmwareResponse.hpp"
#include "Models/Json.hpp"
#include "Models/ReadableArchive.hpp"
#include "Models/ReadableBookmark.hpp"
#include "Models/ReadablePlacemark.hpp"
#include "Models/SyncBookmarksResponse.hpp"
#include "Models/UserReadable.hpp"
#include "Storage/FileSystem.hpp"
#include "Storage/PreferencesStore.hpp"

class SindarinAPI {
public:
    SindarinAPI() = default;
    SindarinAPI(SindarinAPI const &) = delete;

    static auto getInstance() -> std::shared_ptr<SindarinAPI>;
    static auto isSindarinUrl(const char *url) -> bool;

    void setAccessToken(const std::string &accessToken);

    auto checkSync(CheckSyncRequest &req, CheckSyncResponse *resp) -> int;
    auto checkSync(const std::string &url, CheckSyncRequest &req, CheckSyncResponse *resp) -> int;
    auto checkFirmware(FirmwareResponse *resp) -> int;

    auto downloadReadable(const Readable &readable, FileSystem &fileSystem, const std::string &path,
                          const std::function<void(int percentage)> &progressCallback) -> bool;

    auto uploadAndResetVoltageLog(FileSystem &fileSystem) -> int;

    auto uploadCrashReport(SFile &crashFile, const std::string &contentType) -> int;

    void setAuth(HttpClient &client);

protected:
    static std::shared_ptr<SindarinAPI> instance;
    std::string accessToken_;
};
