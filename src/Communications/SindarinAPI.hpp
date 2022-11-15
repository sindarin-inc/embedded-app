#pragma once
#include <ArduinoJson.h>
#include <HTTPClient.h>

#include "Models/CheckSyncResponse.hpp"
#include "Models/Firmware.hpp"
#include "Models/Json.hpp"
#include "Models/UserReadable.hpp"
#include "Storage/FileSystem.hpp"

class SindarinAPI {
public:
    static auto getInstance() -> SindarinAPI *;

    void setAccessToken(std::string accessToken);

    auto beginClient(HTTPClient &httpClient, const String &urlPath) -> bool;

    auto checkSync(const std::string &readablesSha, CheckSyncResponse *resp) -> int;

    // Get list of readables from the server
    auto getUserReadables(std::vector<UserReadable> *userReadables) -> int;

    auto downloadReadable(const Readable &readable, FileSystem &fileSystem,
                          const std::function<void(int percentage)> &progressCallback) -> bool;

    auto uploadAndResetVoltageLog(FileSystem &fileSystem) -> int;

protected:
    static SindarinAPI *instance;
    std::string accessToken_;
};
