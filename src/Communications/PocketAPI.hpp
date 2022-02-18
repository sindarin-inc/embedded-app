#pragma once
#ifndef PocketAPI_H
#define PocketAPI_H
#include <ArduinoJson.h>
#include <HTTPClient.h>

#include "Contents/Json.hpp"

class PocketAPI {
public:
    static auto getInstance() -> PocketAPI *;

    void setAccessToken(std::string accessToken);

    // https://text.getpocket.com/v3/text API
    // Docs: https://docs.google.com/document/d/1R4weidHWfRXjOx8BEROE01ZNQwIGna7KXbIUKH2rzQ0/edit
    // Note: the JsonDocument needs to be freed by the caller
    auto text(const char *url, SpiRamJsonDocument **response) -> int;

    // https://getpocket.com/v3/get API
    // Docs: https://getpocket.com/developer/docs/v3/retrieve
    // Note: the JsonDocument needs to be freed by the caller
    auto get(SpiRamJsonDocument **response) -> int;

protected:
    static PocketAPI *instance;
    std::string accessToken_;
    const char *consumerKey_ = "98329-339ec9babc792e8024ca00e8";
};

#endif