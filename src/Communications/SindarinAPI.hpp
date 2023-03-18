#pragma once
#include <ArduinoJson.h>
#include <esp_http_client.h>
#include <functional>

#include "Communications/HttpClient.hpp"
#include "Models/CheckSyncResponse.hpp"
#include "Models/Firmware.hpp"
#include "Models/FirmwareResponse.hpp"
#include "Models/Json.hpp"
#include "Models/ReadableArchive.hpp"
#include "Models/ReadableBookmark.hpp"
#include "Models/ReadablePlacemark.hpp"
#include "Models/SyncBookmarksResponse.hpp"
#include "Models/UserReadable.hpp"
#include "Storage/FileSystem.hpp"

class SindarinAPI {
public:
    static auto getInstance() -> std::shared_ptr<SindarinAPI>;
    static auto isSindarinUrl(const char *url) -> bool;

    void setAccessToken(const std::string &accessToken);

    auto checkSync(const char *req, size_t bodyLength, CheckSyncResponse *resp,
                   std::string modelType) -> int;
    auto checkSync(std::string url, const char *body, size_t bodyLength, CheckSyncResponse *resp,
                   std::string modelType) -> int;
    auto checkFirmware(FirmwareResponse *resp) -> int;

    auto syncBookmarks(const char *bookmarksJson, const int bookmarksSize) -> int;
    auto downloadReadable(const Readable &readable, FileSystem &fileSystem, const std::string &path,
                          const std::function<void(int percentage)> &progressCallback) -> bool;

    auto uploadAndResetVoltageLog(FileSystem &fileSystem) -> int;

    auto uploadCrashReport(File &crashFile, const String &contentType) -> int;

    void setAuth(HttpClient &client);

protected:
    static std::shared_ptr<SindarinAPI> instance;
    std::string accessToken_;
};
