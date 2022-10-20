#include "SindarinAPI.hpp"

#include <utility>

#include "FileDownload.hpp"
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
    auto string = httpClient.getString();
    auto *respJson = new SpiRamJsonDocument(string.length());
    ArduinoJson::deserializeJson(*respJson, string);
    resp->fromJson(respJson->as<JsonObject>());
    log_d("Synced: %d Message: %s, Firmware: %s", resp->synced, resp->message.c_str(),
          resp->firmware.url.c_str());
    delete respJson;

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
        delete resp;

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

auto SindarinAPI::downloadReadable(Readable &readable, FileSystem &fileSystem,
                                   const std::function<void(int percentage)> &progressCallback)
    -> bool {
    HTTPClient httpClient;
    bool success = false;

    auto remoteUrl = readable.remoteUrl();
    if (!httpClient.begin(remoteUrl.c_str())) {
        log_e("HTTPClient::begin failed for URL %s", remoteUrl.c_str());
        return false;
    }
    httpClient.setFollowRedirects(HTTPC_FORCE_FOLLOW_REDIRECTS);
    httpClient.setTimeout(30000);

    auto localPath = readable.localFilePath();
    unsigned long downloadStart = millis();

    log_d("Downloading readable from %s to %s", remoteUrl.c_str(), localPath.c_str());
    int httpCode = httpClient.GET();
    // HTTP header has been sent and Server response header has been handled
    // file found at server
    if (httpCode != HTTP_CODE_OK) {
        log_e("[HTTP] GET... failed, error: %s\n", httpClient.errorToString(httpCode).c_str());
    } else {
        // Get length of document (is -1 when Server sends no Content-Length header)
        int len = httpClient.getSize();
        if (len != readable.file().size) {
            log_w("Readable Content-Length is %d but expected size was %d", len,
                  readable.file().size);
            len = readable.file().size;
        } else {
            log_d("Readable length is %d", len);
        }

        // TODO: This is kinda weird to get the filesystem from localstore
        success =
            DownloadFile(httpClient, fileSystem, localPath.c_str(), len,
                         [downloadStart, progressCallback](int bytesWritten, float percentage) {
                             log_d("Downloaded %.1f%% (%.1f KB/s)", percentage,
                                   bytesWritten / 1024.0f / ((millis() - downloadStart) / 1000.0f));
                             progressCallback(percentage);
                         });
    }
    httpClient.end();
    return success;
}
