#include "SindarinAPI.hpp"

#include <utility>

#include "GitVersion.hpp"
#include "Misc/StringUtil.hpp"
#include "Models/Json.hpp"
#include "Models/UserReadable.hpp"
#include "UrlEncode.hpp"
#include "Version.hpp"
#include "sindarin-debug.h"

#define SINDARIN_SERVER_URL "https://sol-cloud.fly.dev"

SindarinAPI *SindarinAPI::instance = nullptr;

auto SindarinAPI::getInstance() -> SindarinAPI * {
    if (instance == nullptr) {
        instance = new SindarinAPI();
    }
    return instance;
}

auto SindarinAPI::beginClient(HTTPClient &httpClient, const String &urlPath) -> bool {
    if (!httpClient.begin(String(SINDARIN_SERVER_URL) + urlPath)) {
        // TODO: When can this happen?
        log_e("HTTPClient::begin failed");
        return false;
    }
    httpClient.setUserAgent("Sindarin/" + String(kVersionString) + " (" + String(kGitHash) + ")");
    httpClient.setFollowRedirects(HTTPC_FORCE_FOLLOW_REDIRECTS);
    httpClient.addHeader("Content-Type", "application/json");
    httpClient.addHeader("Authorization", "Tolkien " + String(accessToken_.c_str()));
    httpClient.setTimeout(30000);
    return true;
}

void SindarinAPI::setAccessToken(std::string token) { accessToken_ = std::move(token); }

auto SindarinAPI::checkSync(const std::string &readablesSha, CheckSyncResponse *resp) -> int {
    HTTPClient httpClient;
    auto params = StringFormat("/sync/all/?sha=%s&build=%s&git_hash=%s", readablesSha.c_str(),
                               kVersionString, kGitHash);
    if (!beginClient(httpClient, String(params.c_str()))) {
        return -1;
    }

    int httpCode = httpClient.GET();

    if (httpCode == HTTP_CODE_OK) {
        log_i("Client is in sync");
        resp->synced = true;
    } else if (httpCode == 419) {
        log_i("Client is out of date");
        resp->synced = false;
    } else {
        log_e("GET /sync/all failed");
        httpClient.end();
        return httpCode;
    }

    auto string = httpClient.getString();
    auto *respJson = new SpiRamJsonDocument(string.length());
    ArduinoJson::deserializeJson(*respJson, string);
    resp->message = (*respJson)["message"].as<std::string>();
    resp->firmware.fromJson((*respJson)["firmware"]);
    log_d("Message: %s, Firmware: %s", resp->message.c_str(), resp->firmware.url.c_str());

    httpClient.end();
    return httpCode;
}

auto SindarinAPI::getReadablesUsersAll(std::vector<UserReadable> *userReadables) -> int {
    HTTPClient httpClient;
    if (!beginClient(httpClient, String("/readables/users/all/"))) {
        return -1;
    }

    int httpCode = httpClient.GET();
    if (httpCode == HTTP_CODE_OK) {
        auto string = httpClient.getString();
        // The document seems to need more length than the JSON string itself, so to be safe we just
        // double the storage. We have plenty of SPIRAM anyways.
        auto *resp = new SpiRamJsonDocument(string.length() * 2);
        ArduinoJson::deserializeJson(*resp, string);
        log_d("%s", string.c_str());

        // TODO: Handle pagination
        // auto count = (*resp)["count"].as<int>();
        // auto next = (*resp)["next"].as<std::string>();
        // auto previous = (*resp)["previous"].as<std::string>();

        JsonArray userReadablesJson = (*resp)["results"].as<JsonArray>();

        for (JsonObject userReadableJson : userReadablesJson) {
            UserReadable userReadable;
            if (!userReadable.fromJson(userReadableJson)) {
                log_e("Failed to parse UserReadable");
                continue;
            }
            userReadables->push_back(userReadable);
        }

        log_i("GET /readables/users/all succeeded");
    } else {
        log_w("GET /readables/users/all request failed with code %d\n", httpCode);
    }
    httpClient.end();
    return httpCode;
}

auto SindarinAPI::downloadReadable(Readable &readable, LocalStore &localStore) -> int {
    HTTPClient httpClient;
    auto remoteUrl = readable.remoteUrl();
    if (!httpClient.begin(remoteUrl.c_str())) {
        log_e("HTTPClient::begin failed for URL %s", remoteUrl.c_str());
        return -1;
    }
    httpClient.setFollowRedirects(HTTPC_FORCE_FOLLOW_REDIRECTS);
    httpClient.setTimeout(30000);

    auto localPath = readable.localFilePath();

    log_d("Downloading readable from %s to %s", remoteUrl.c_str(), localPath.c_str());
    int httpCode = httpClient.GET();
    // HTTP header has been send and Server response header has been handled
    // file found at server
    if (httpCode == HTTP_CODE_OK) {
        // Get length of document (is -1 when Server sends no Content-Length header)
        int len = httpClient.getSize();
        if (len != readable.file().size) {
            log_w("Readable Content-Length is %d but expected size was %d", len,
                  readable.file().size);
        } else {
            log_d("Readable length is %d", len);
        }

        // TODO: This is kinda weird to get the filesystem from localstore
        auto file = localStore.fileSystem.open(localPath, "w");

        // W25Q512JV* chips have a page size of 256 bytes so that seems like a good value to use
        // here.
        uint8_t buff[256] = {0};

        auto *stream = httpClient.getStreamPtr();

        // Read all data from server
        while (stream->available() && (len > 0 || len == -1)) {
            int c = stream->readBytes(buff, sizeof(buff));
            file.write(buff, c);
            if (len > 0) {
                len -= c;
            }
            // Let other tasks do stuff.
            vPortYield();
        }
        file.close();
        log_d("File successfully downloaded.");

        localStore.addReadable(readable);
    } else {
        log_d("[HTTP] GET... failed, error: %s\n", httpClient.errorToString(httpCode).c_str());
    }

    httpClient.end();
    return httpCode;
}
