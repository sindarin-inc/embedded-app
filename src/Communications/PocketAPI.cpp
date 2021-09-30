#include "PocketAPI.hpp"
#include "sindarin-debug.h"
#include "Contents/Json.hpp"
#include "UrlEncode.hpp"

PocketAPI *PocketAPI::instance = nullptr;

PocketAPI *PocketAPI::getInstance()
{
    if (instance == nullptr)
    {
        instance = new PocketAPI();
    }
    return instance;
}

void PocketAPI::setAccessToken(const char *token) {
    access_token = token;
}

int PocketAPI::text(const char *url, SpiRamJsonDocument **response) {
    SHOW_HEAP();
    http.begin("https://text.getpocket.com/v3/text");
    http.addHeader("Content-Type", "application/x-www-form-urlencoded");
    http.useHTTP10(true);

    /*
        curl -v \
        -d "consumer_key=$POCKET_CONSUMER_KEY&url=$POCKET_URL" \
        https://text.getpocket.com/v3/text | jq
    */
    char request[200];
    int request_len = sprintf(request, "consumer_key=%s&url=%s", consumer_key, urlencode(url).c_str());
    if (request_len < -1) {
        log_e("Failed to construct Pocket request body");
        http.end();
        return request_len;
    }

    // TODO: create this statically?
    StaticJsonDocument<200> filter;
    filter["excerpt"] = true;
    filter["article"] = true;

    int http_code = http.POST((uint8_t *)request, request_len);
    if (http_code == 200) {
        SpiRamJsonDocument *resp = new SpiRamJsonDocument(16384);
        ArduinoJson::deserializeJson(*resp, http.getStream(), DeserializationOption::Filter(filter));
        log_i("Pocket request succeeded");
        *response = resp;
    } else {
        log_w("Pocket request failed with code %d\n", http_code);
    }
    SHOW_HEAP();
    http.end();
    SHOW_HEAP();
    return http_code;
}

int PocketAPI::get(SpiRamJsonDocument **response) {
    SHOW_HEAP();
    http.begin("https://getpocket.com/v3/get");
    http.addHeader("Content-Type", "application/x-www-form-urlencoded");
    http.useHTTP10(true);

    /*
        curl -v \
        -d "state=all&access_token=$POCKET_ACCESS_TOKEN&consumer_key=$POCKET_CONSUMER_KEY" \
        https://getpocket.com/v3/get | jq
    */
    char request[100];
    int request_len = sprintf(request, "state=all&access_token=%s&consumer_key=%s", access_token, consumer_key);
    if (request_len < -1) {
        log_e("Failed to construct Pocket request body");
        http.end();
        return request_len;
    }

    // TODO: create this statically?
    StaticJsonDocument<200> filter;
    filter["list"]["*"]["resolved_title"] = true;
    filter["list"]["*"]["resolved_id"] = true;
    filter["list"]["*"]["resolved_url"] = true;

    int http_code = http.POST((uint8_t *)request, request_len);
    if (http_code == 200) {
        SpiRamJsonDocument *resp = new SpiRamJsonDocument(2048);
        ArduinoJson::deserializeJson(*resp, http.getStream(), DeserializationOption::Filter(filter));
        log_i("Pocket request succeeded");
        *response = resp;
    } else {
        log_w("Pocket request failed with code %d", http_code);
    }
    SHOW_HEAP();
    http.end();
    SHOW_HEAP();
    return http_code;
}

int PocketAPI::getFirstArticleText(const char **text) {
    SHOW_HEAP();
    SpiRamJsonDocument *response;
    int code = this->get(&response);
    if (code != 200) {
        return code;
    }
    log_i("Pocket /get request succeeded");

    // Grab the first article
    const char *url = "";
    const char *title = "";
    JsonObject list = (*response)["list"].as<JsonObject>();
    if (list.size() <= 0) {
        log_e("No articles found on Pocket");
        return 404;
    }
    for (JsonPair keyValue : list) {
        JsonObject article = keyValue.value().as<JsonObject>();
        title = article["resolved_title"].as<const char*>();
        url = article["resolved_url"].as<const char*>();
        break;
    }
    delete response;

    log_i("Grabbing Pocket article \"%s\"", title);

    // Grab the article text
    code = this->text(url, &response);
    if (code != 200) {
        return code;
    }

    log_i("Pocket /text request succeeded");
    *text = (*response)["article"].as<const char *>();

    delete response;

    return 200;
}
