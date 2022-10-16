#pragma once
#include <ArduinoJson.h>
#include <HTTPClient.h>

#include "Models/Firmware.hpp"
#include "Models/Json.hpp"
#include "Models/UserReadable.hpp"
#include "Storage/LocalStore.hpp"

struct CheckSyncResponse {
    bool synced;
    std::string message;
    Firmware firmware;
};

class SindarinAPI {
public:
    static auto getInstance() -> SindarinAPI *;

    void setAccessToken(std::string accessToken);

    auto beginClient(HTTPClient &httpClient, const String &urlPath) -> bool;

    auto checkSync(const std::string &readablesSha, CheckSyncResponse *resp) -> int;

    // Get list of readables from the server
    auto getReadablesUsersAll(std::vector<UserReadable> *userReadables) -> int;

    auto downloadReadable(Readable &readable, LocalStore &localStore) -> int;

protected:
    static SindarinAPI *instance;
    std::string accessToken_;
};
