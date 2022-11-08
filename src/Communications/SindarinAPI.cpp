#include "SindarinAPI.hpp"

#include <utility>

#include "FileDownload.hpp"
#include "GitVersion.hpp"
#include "Misc/Power.hpp"
#include "Misc/StringUtil.hpp"
#include "Models/Json.hpp"
#include "Models/UserReadable.hpp"
#include "UrlEncode.hpp"
#include "Version.hpp"
#include "sindarin-debug.h"

#define SINDARIN_SERVER_URL "https://sol-cloud.fly.dev"
// #define SINDARIN_SERVER_URL "https://sol-staging.fly.dev"

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
    if (!accessToken_.empty()) {
        httpClient.addHeader("Authorization", "Tolkien " + String(accessToken_.c_str()));
    }
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
    auto *respJson = new SpiRamJsonDocument(string.length() * 2);
    auto error = ArduinoJson::deserializeJson(*respJson, string);
    if (error.code() != DeserializationError::Ok) {
        log_e("Failed to deserialize sync response: %s", error.c_str());
        delete respJson;
        httpClient.end();
        return 0;
    }
    resp->fromJson(respJson->as<JsonObject>());
    log_d("Synced: %d Message: %s, Firmware: %s", resp->synced, resp->message.c_str(),
          resp->firmware.url.c_str());
    delete respJson;

    httpClient.end();
    return httpCode;
}

auto SindarinAPI::getUserReadables(std::vector<UserReadable> *userReadables) -> int {
    HTTPClient httpClient;
    if (!beginClient(httpClient, String("/user/readables/"))) {
        return -1;
    }

    int httpCode = httpClient.GET();
    if (httpCode == HTTP_CODE_OK) {
        auto string = httpClient.getString();
        // The document seems to need more length than the JSON string itself, so to be safe we just
        // double the storage. We have plenty of SPIRAM anyways.
        auto *resp = new SpiRamJsonDocument(string.length() * 2);
        auto error = ArduinoJson::deserializeJson(*resp, string);
        if (error.code() != DeserializationError::Ok) {
            log_e("Failed to deserialize user readables: %s", error.c_str());
            delete resp;
            httpClient.end();
            return 0;
        }

        log_d("%s", string.c_str());

        // TODO: Handle pagination
        // auto count = (*resp)["count"].as<int>();
        // auto next = (*resp)["next"].as<std::string>();
        // auto previous = (*resp)["previous"].as<std::string>();

        JsonArray userReadablesJson = (*resp)["results"].as<JsonArray>();
        delete resp;

        for (JsonObject userReadableJson : userReadablesJson) {
            userReadables->emplace_back();
            if (!userReadables->back().fromJson(userReadableJson)) {
                log_e("Failed to parse UserReadable");
                continue;
            }
        }

        log_i("GET /user/readables/ succeeded");
    } else {
        log_w("GET /user/readables/ request failed with code %d\n", httpCode);
    }
    httpClient.end();
    return httpCode;
}

auto SindarinAPI::downloadReadable(const Readable &readable, FileSystem &fileSystem,
                                   const std::function<void(int percentage)> &progressCallback)
    -> bool {
    HTTPClient httpClient;
    bool success = false;

    auto remoteUrl = readable.remoteUrl();
    if (!httpClient.begin(remoteUrl.c_str())) {
        log_e("HTTPClient::begin failed for URL %s", remoteUrl.c_str());
        return false;
    }
    httpClient.setUserAgent("Sindarin/" + String(kVersionString) + " (" + String(kGitHash) + ")");
    httpClient.setFollowRedirects(HTTPC_FORCE_FOLLOW_REDIRECTS);
    httpClient.setTimeout(30000);

    auto localPath = readable.localFilePath();

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

        success = DownloadFile(httpClient, fileSystem, localPath.c_str(), len,
                               [progressCallback](int bytesWritten, float percentage) {
                                   progressCallback(percentage);
                               });
    }
    httpClient.end();
    return success;
}

auto SindarinAPI::uploadAndResetVoltageLog(FileSystem &fileSystem) -> int {
    HTTPClient httpClient;

    auto localPath = Power::volatageLogPath();
    if (!fileSystem.exists(localPath)) {
        log_w("Voltage log file %s does not exist", localPath);
        return false;
    }
    auto file = fileSystem.open(localPath, "r");

    if (!beginClient(httpClient, "/user/voltagelogs/")) {
        file.close();
        return -1;
    }

    auto requestJson = std::make_unique<SpiRamJsonDocument>(file.size() * 2);
    (*requestJson)["voltage_log"] = file.readString();

    if ((*requestJson).overflowed()) {
        log_e("Failed to create JSON for voltage log upload");
        file.close();
        httpClient.end();
        return -1;
    }

    // This can be pretty big, so easiest to just dump it to a file then use the File's Stream to
    // send. Also HTTPClient has an easy stream uploader that works with File Streams.
    // TODO: It would be better to use a stream in SPIRAM directly instead of dumping to a file to
    // avoid unnecessary flash writes.
    auto jsonFilePath = "/logs/voltagelog.json";
    File jsonFile = fileSystem.open(jsonFilePath, "rw", true);
    serializeJson(*requestJson, jsonFile);

    // Reset the file back to its start so that we can reuse it for sending
    jsonFile.flush();
    jsonFile.seek(0);

    auto httpCode = httpClient.sendRequest("POST", &jsonFile, jsonFile.size());

    httpClient.end();
    jsonFile.close();

    fileSystem.remove(jsonFilePath);

    return httpCode;
}
