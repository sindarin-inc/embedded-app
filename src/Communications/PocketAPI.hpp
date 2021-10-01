#pragma once
#ifndef PocketAPI_H
#define PocketAPI_H
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include "Contents/Json.hpp"

#define POCKET_API_TOKEN_PREFS_KEY "pocket-token"
class PocketAPI
{
public:
    static PocketAPI *getInstance();

    void setAccessToken(const char *access_token);
    // https://text.getpocket.com/v3/text API
    // Docs: https://docs.google.com/document/d/1R4weidHWfRXjOx8BEROE01ZNQwIGna7KXbIUKH2rzQ0/edit
    // Note: the JsonDocument needs to be freed by the caller
    int text(const char *url, SpiRamJsonDocument **response);

    // https://getpocket.com/v3/get API
    // Docs: https://getpocket.com/developer/docs/v3/retrieve
    // Note: the JsonDocument needs to be freed by the caller
    int get(SpiRamJsonDocument **response);

    // Convenience method to grab
    int getFirstArticleText(const char **text);

protected:
    static PocketAPI * instance;
    const char * access_token;
    const char * consumer_key = "98329-339ec9babc792e8024ca00e8";

private:
    HTTPClient http;
};

#endif