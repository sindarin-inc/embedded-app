#if ENABLE_POCKET

#include "PocketAPI.hpp"

#include <utility>

#include "Models/Json.hpp"
#include "UrlEncode.hpp"
#include "sindarin-debug.h"

PocketAPI *PocketAPI::instance = nullptr;

auto PocketAPI::getInstance() -> PocketAPI * {
    if (instance == nullptr) {
        instance = new PocketAPI();
    }
    return instance;
}

void PocketAPI::setAccessToken(std::string token) { accessToken_ = std::move(token); }

auto PocketAPI::text(const char *url, SpiRamJsonDocument **response) -> int {
    SHOW_HEAP();

    HTTPClient httpClient;
    if (!httpClient.begin("https://text.getpocket.com/v3/text")) {
        log_e("HTTPClient::begin failed");
        return -1;
    }
    httpClient.addHeader("Content-Type", "application/x-www-form-urlencoded");
    httpClient.setTimeout(30000);

    /*
        curl -v \
        -d "consumer_key=$POCKET_CONSUMER_KEY&url=$POCKET_URL" \
        https://text.getpocket.com/v3/text | jq
    */
    char request[300];
    int requestLen = snprintf(request, sizeof(request), "consumer_key=%s&url=%s", consumerKey_,
                              Urlencode(url).c_str());
    if (requestLen < -1) {
        log_e("Failed to construct Pocket request body");
        httpClient.end();
        return requestLen;
    }

    // TODO: create this statically?
    StaticJsonDocument<200> filter;
    filter["excerpt"] = true;
    filter["article"] = true;
    filter["title"] = true;
    filter["resolved_id"] = true;

    int httpCode = httpClient.POST((uint8_t *)request, requestLen);
    if (httpCode == 200) {
        auto string = httpClient.getString();
        auto *resp = new SpiRamJsonDocument(string.length());
        ArduinoJson::deserializeJson(*resp, string, DeserializationOption::Filter(filter));
        log_i("Pocket /text succeeded for %s", url);
        *response = resp;
    } else {
        log_w("Pocket request failed with code %d\n", httpCode);
    }
    SHOW_HEAP();
    httpClient.end();
    SHOW_HEAP();
    return httpCode;
}

auto PocketAPI::get(SpiRamJsonDocument **response) -> int {
    SHOW_HEAP();
    HTTPClient httpClient;

    if (!httpClient.begin("https://getpocket.com/v3/get")) {
        log_e("HTTPClient::begin failed");
        return -1;
    }
    httpClient.addHeader("Content-Type", "application/x-www-form-urlencoded");

    /*
        curl -v \
        -d "state=all&access_token=$POCKET_ACCESS_TOKEN&consumer_key=$POCKET_CONSUMER_KEY" \
        https://getpocket.com/v3/get | jq
    */
    char request[300];
    int requestLen = snprintf(request, sizeof(request), "state=all&access_token=%s&consumer_key=%s",
                              accessToken_.c_str(), consumerKey_);
    if (requestLen < -1) {
        log_e("Failed to construct Pocket request body");
        httpClient.end();
        return requestLen;
    }

    // TODO: create this statically?
    StaticJsonDocument<200> filter;
    filter["list"]["*"]["resolved_title"] = true;
    filter["list"]["*"]["resolved_id"] = true;
    filter["list"]["*"]["resolved_url"] = true;

    int httpCode = httpClient.POST((uint8_t *)request, requestLen);
    if (httpCode == 200) {
        auto string = httpClient.getString();
        auto *resp = new SpiRamJsonDocument(string.length());
        ArduinoJson::deserializeJson(*resp, string, DeserializationOption::Filter(filter));
        log_i("Pocket /get succeeded");
        *response = resp;
    } else {
        log_w("Pocket request failed with code %d", httpCode);
    }
    SHOW_HEAP();
    httpClient.end();

    SHOW_HEAP();
    return httpCode;
}

#endif
