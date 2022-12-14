#pragma once
#include <ArduinoJson.h>
#include <HTTPClient.h>

#include "Models/CheckSyncResponse.hpp"
#include "Models/Firmware.hpp"
#include "Models/Json.hpp"
#include "Models/UserReadable.hpp"
#include "Storage/FileSystem.hpp"

struct CheckSyncRequest {
    std::string readablesSince;
    std::string articlesSince;
    std::vector<std::string> readablePlacemarks;
    std::vector<std::string> articlePlacemarks;
    std::vector<std::string> articlesArchived;
    int ptBattery;
    std::string ptBuild;
};

class SindarinAPI {
public:
    static auto getInstance() -> SindarinAPI *;

    void setAccessToken(std::string accessToken);

    auto beginClient(HTTPClient &httpClient, const String &urlPath) -> bool;
    auto beginClient(HTTPClient &httpClient, const String &urlPath, const String &contentType)
        -> bool;

    auto checkSync(const CheckSyncRequest &req, CheckSyncResponse *resp) -> int;

    // Get list of readables from the server
    auto getUserReadables(std::vector<UserReadable> *userReadables) -> int;

    auto downloadReadable(const Readable &readable, FileSystem &fileSystem,
                          const std::function<void(int percentage)> &progressCallback) -> bool;

    auto uploadAndResetVoltageLog(FileSystem &fileSystem) -> int;

    auto uploadCrashReport(File crashFile, const String &contentType) -> int;

protected:
    static SindarinAPI *instance;
    std::string accessToken_;
};
