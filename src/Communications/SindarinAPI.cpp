#include "SindarinAPI.hpp"

#include <utility>

#include "FileDownload.hpp"
#include "GitVersion.hpp"
#include "Misc/Platform.hpp"
#include "Misc/Power.hpp"
#include "Misc/StringUtil.hpp"
#include "Models/Json.hpp"
#include "Models/UserReadable.hpp"
#include "UrlEncode.hpp"
#include "Version.hpp"
#include "sindarin-debug.h"

#define SINDARIN_SERVER_URL "https://api.sindarin.com"
//#define SINDARIN_SERVER_URL "https://staging.sindarin.com"

std::shared_ptr<SindarinAPI> SindarinAPI::instance = nullptr;

auto SindarinAPI::getInstance() -> std::shared_ptr<SindarinAPI> {
    if (instance == nullptr) {
        instance = std::make_shared<SindarinAPI>();
    }
    return instance;
}

void SindarinAPI::setAuth(HttpClient &client) {
    if (!accessToken_.empty()) {
        esp_err_t err = client.setHeader("Authorization",
                                         StringFormat("Tolkien %s", accessToken_.c_str()).c_str());
        if (err != ESP_OK) {
            log_e("Failed addHeader with %s", esp_err_to_name(err));
        }
    }
}

void SindarinAPI::setAccessToken(const std::string &token) { accessToken_ = token; }

auto SindarinAPI::checkSync(const CheckSyncRequest &req, CheckSyncResponse *resp) -> int {
    HttpClient client(SINDARIN_SERVER_URL "/sync/all/", "application/x-www-form-urlencoded",
                      HTTP_METHOD_POST);
    setAuth(client);

    auto body = StringFormat("build=%s&git_hash=%s&sn=%"
                             "s&pt_battery=%d&pt_build=%s&skip_content=%s",
                             kVersionString, kGitHash, GetSerialNumber().c_str(), req.ptBattery,
                             req.ptBuild.c_str(), req.skipContent ? "true" : "false");
    if (req.readablesSince.length() > 0) {
        body += StringFormat("&readables_since=%s", req.readablesSince.c_str());
    }
    if (req.articlesSince.length() > 0) {
        body += StringFormat("&articles_since=%s", req.articlesSince.c_str());
    }
    for (auto &placemark : req.readablePlacemarks) {
        body += placemark.c_str();
    }
    for (auto &placemark : req.articlePlacemarks) {
        body += placemark.c_str();
    }
    for (auto &readable : req.readablesArchived) {
        body += readable.c_str();
    }
    for (auto &article : req.articlesArchived) {
        body += article.c_str();
    }

    client.open(body.length());
    int writeLength = client.write(body.c_str(), body.length());
    if (writeLength < body.length()) {
        log_e("HttpClient failed to write (writeLength=%d)", writeLength);
        return 0;
    }

    int contentLength = client.fetchHeaders();
    int httpCode = client.getStatusCode();
    auto stream = client.makeReadStream();

    SHOW_HEAP();

    auto respJson = std::make_unique<SpiRamJsonDocument>(1024 + contentLength * 3);
    auto error = ArduinoJson::deserializeJson(*respJson, *stream);

    SHOW_HEAP();

    if (error.code() != DeserializationError::Ok) {
        log_e("Failed to deserialize sync response: %s (length=%d)", error.c_str(), contentLength);
        return 0;
    }

    SHOW_HEAP();

    resp->fromJson(respJson->as<JsonObject>());

    SHOW_HEAP();

    log_d("Synced: %d Message: %s, Firmware: %s", resp->synced, resp->message.c_str(),
          resp->firmware.url.c_str());

    SHOW_HEAP();

    return httpCode;
}

auto SindarinAPI::syncBookmarks(const char *bookmarksJson, const int bookmarksSize) -> int {
    SyncBookmarksResponse resp;
    HttpClient client(SINDARIN_SERVER_URL "/user/bookmarks/", "application/json", HTTP_METHOD_POST);
    setAuth(client);

    client.open(bookmarksSize);
    int writeLength = client.write(bookmarksJson, bookmarksSize);
    if (writeLength < bookmarksSize) {
        log_e("HttpClient failed to write (writeLength=%d)", writeLength);
        return 0;
    }

    int contentLength = client.fetchHeaders();
    int httpCode = client.getStatusCode();
    auto stream = client.makeReadStream();

    SHOW_HEAP();

    auto respJson = std::make_unique<SpiRamJsonDocument>(1024 + contentLength * 3);
    auto error = ArduinoJson::deserializeJson(*respJson, *stream);

    SHOW_HEAP();

    if (error.code() != DeserializationError::Ok) {
        log_e("Failed to deserialize sync bookmarks response: %s (length=%d)", error.c_str(),
              contentLength);
        return 0;
    }

    SHOW_HEAP();

    resp.fromJson(respJson->as<JsonObject>());

    SHOW_HEAP();

    return httpCode;
}

auto SindarinAPI::downloadReadable(const Readable &readable, FileSystem &fileSystem,
                                   const std::function<void(int percentage)> &progressCallback,
                                   bool gzip) -> bool {
    bool success = false;
    // auto remoteUrl = readable.remoteUrl();
    auto remoteUrl = readable.file().fileProxied;
    auto localPath = readable.localFilePath();
    auto fileSize = readable.file().size;

    success = DownloadFile(
        remoteUrl.c_str(), fileSystem, localPath.c_str(), fileSize,
        [progressCallback](float percentage) { progressCallback(percentage); }, gzip);

    return success;
}

auto SindarinAPI::uploadAndResetVoltageLog(FileSystem &fileSystem) -> int {
    auto localPath = Power::voltageLogPath();
    if (!fileSystem.exists(localPath)) {
        log_w("Voltage log file %s does not exist", localPath);
        return false;
    }
    auto file = fileSystem.open(localPath, FILE_READ);

    auto requestJson = std::make_unique<SpiRamJsonDocument>(file.size() * 2);
    (*requestJson)["voltage_log"] = file.readString();
    file.close();

    if ((*requestJson).overflowed()) {
        log_e("Failed to create JSON for voltage log upload");
        return -1;
    }

    // `memoryUsage()` isn't quite the right amount and we can't know for sure how big the
    // serialized data will be until we actually serialize it. We probably only need 20% more or so
    // but we have plenty of SPIRAM so let's just double it.
    size_t serializedMaxSize = (*requestJson).memoryUsage() * 2;
    char *serialized = (char *)heap_caps_malloc(serializedMaxSize, MALLOC_CAP_SPIRAM);

    // Write to SPIRAM
    // TODO: I can't find a good way to verify that the whole thing was written. Maybe would be
    // better for this to be a stream so we could check if we attempted to write but ran out of
    // space.
    size_t serializedLength = serializeJson(*requestJson, serialized, serializedMaxSize);
    if (serializedLength == serializedMaxSize) {
        log_e("serializedLength is the same as the max size, probably ran out of space");
    }

    HttpClient client(SINDARIN_SERVER_URL "/user/voltagelogs/", "application/json",
                      HTTP_METHOD_POST);
    setAuth(client);

    auto err = client.open(serializedLength);
    if (err != ESP_OK) {
        log_e("HttpClient failed to open: %d", err);
        heap_caps_free(serialized);
        return -1;
    }

    int writeLength = client.write(serialized, serializedLength);
    if (writeLength != serializedLength) {
        log_e("HttpClient failed to write fully (writeLength=%d, serializedLength=%zu)",
              writeLength, serializedLength);
    }

    heap_caps_free(serialized);

    client.fetchHeaders();
    int httpCode = client.getStatusCode();

    log_d("Voltage log upload http code: %d", httpCode);

    return httpCode;
}

auto SindarinAPI::uploadCrashReport(File &crashFile, const String &contentType) -> int {
    HttpClient client(SINDARIN_SERVER_URL "/coredumps/", contentType.c_str(), HTTP_METHOD_POST);
    setAuth(client);

    int writeSize = crashFile.size();
    client.open(writeSize);

    size_t bufferSize = 4096;
    char *buffer = (char *)heap_caps_malloc(bufferSize, MALLOC_CAP_SPIRAM);

    // NOTE: crashFile.available() never goes down but it seems like it should
    int bytesWritten = 0;
    while (bytesWritten < writeSize) {
        auto bytesRead = crashFile.readBytes(buffer, bufferSize);
        bytesWritten += client.write(buffer, bytesRead);
    }

    heap_caps_free(buffer);

    client.fetchHeaders();
    int httpCode = client.getStatusCode();

    log_d("uploadCrashReport http code %d", httpCode);

    return httpCode;
}
