#pragma once
#include <ArduinoJson.h>
#include <esp_http_client.h>
#include <functional>

#include "Communications/HttpClient.hpp"
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
    std::vector<std::string> readablesArchived;
    std::vector<std::string> articlesArchived;
    int ptBattery;
    std::string ptBuild;
    bool skipContent;
};

class SindarinAPI {
public:
    static auto getInstance() -> std::shared_ptr<SindarinAPI>;

    void setAccessToken(const std::string &accessToken);

    auto checkSync(const CheckSyncRequest &req, CheckSyncResponse *resp) -> int;

    auto downloadReadable(const Readable &readable, FileSystem &fileSystem,
                          const std::function<void(int percentage)> &progressCallback, bool gzip)
        -> bool;

    auto uploadAndResetVoltageLog(FileSystem &fileSystem) -> int;

    auto uploadCrashReport(File &crashFile, const String &contentType) -> int;

protected:
    static std::shared_ptr<SindarinAPI> instance;
    std::string accessToken_;

    void setAuth(HttpClient &client);
};
