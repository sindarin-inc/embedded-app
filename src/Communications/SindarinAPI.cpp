#include "SindarinAPI.hpp"

#include <utility>

#include "FileDownload.hpp"
#include "GitVersion.hpp"
#include "Misc/Battery.hpp"
#include "Misc/Platform.hpp"
#include "Misc/StringUtil.hpp"
#include "Models/Json.hpp"
#include "Models/UserReadable.hpp"
#include "UrlEncode.hpp"
#include "Version.hpp"
#include "sindarin-debug.h"

#define SINDARIN_SERVER_URL "https://api.sindarin.com"
// #define SINDARIN_SERVER_URL "https://staging.sindarin.com"

std::shared_ptr<SindarinAPI> SindarinAPI::instance = nullptr;

auto SindarinAPI::getInstance() -> std::shared_ptr<SindarinAPI> {
    if (instance == nullptr) {
        instance = std::make_shared<SindarinAPI>();
    }
    return instance;
}

auto SindarinAPI::isSindarinUrl(const char *url) -> bool {
    return HasPrefix(url, SINDARIN_SERVER_URL);
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

auto SindarinAPI::checkSync(CheckSyncRequest &req, CheckSyncResponse *resp) -> int {
    auto url = StringFormat(SINDARIN_SERVER_URL "/sync/%ss/",
                            ReadableModelTypeToString(req.modelType).c_str());
    log_d("Checking sync for %s: %s", ReadableModelTypeToString(req.modelType).c_str(),
          url.c_str());
    return checkSync(url, req, resp);
}

auto SindarinAPI::checkSync(const std::string &url, CheckSyncRequest &req, CheckSyncResponse *resp)
    -> int {
    HttpClient client(url.c_str(), "application/json", HTTP_METHOD_POST);
    setAuth(client);

    auto doc = std::make_shared<SpiRamJsonDocument>(
        2048 * std::max(1, (int)(req.placemarks.size() + req.archives.size() +
                                 req.bookmarks.size() + req.storedUuids.size())));
    JsonObject body = doc->to<JsonObject>();
    ConvertToJson(req, body);

    size_t serializedMaxSize = (*doc).memoryUsage() * 2;
    char *reqBody = (char *)heap_caps_malloc(serializedMaxSize, MALLOC_CAP_SPIRAM);
    size_t bodyLength = serializeJson(*doc, reqBody, serializedMaxSize);

    client.open(bodyLength);
    int writeLength = client.write(reqBody, bodyLength);
    if (writeLength < bodyLength) {
        log_w("HttpClient failed to write (writeLength=%d, bodyLength=%zu)", writeLength,
              bodyLength);
        return 0;
    }

    int contentLength = client.fetchHeaders();
    if (contentLength <= 0) {
        log_e("Failed to fetch headers");
        heap_caps_free(reqBody);
        return 0;
    }
    int httpCode = client.getStatusCode();
    if (httpCode != 200) {
        log_e("Failed to check sync: %d", httpCode);
        heap_caps_free(reqBody);
        return httpCode;
    }
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

    if (!resp->fromJson(respJson->as<JsonObject>())) {
        log_e("Failed to parse sync response");
        return 0;
    }

    SHOW_HEAP();

    log_d("Synced: %d Message: %s", resp->synced, resp->message.c_str());

    SHOW_HEAP();
    heap_caps_free(reqBody);
    SHOW_HEAP();

    return httpCode;
}

auto SindarinAPI::checkFirmware(FirmwareResponse *resp) -> int {
    HttpClient client(SINDARIN_SERVER_URL "/firmware/");
    setAuth(client);

    client.open(0);

    int contentLength = client.fetchHeaders();
    if (contentLength <= 0) {
        log_e("Failed to fetch headers");
        return 0;
    }
    int httpCode = client.getStatusCode();
    if (httpCode != 200) {
        log_e("Failed to check sync: %d", httpCode);
        return httpCode;
    }
    auto stream = client.makeReadStream();

    SHOW_HEAP();

    auto respJson = std::make_unique<SpiRamJsonDocument>(1024 + contentLength * 3);
    auto error = ArduinoJson::deserializeJson(*respJson, *stream);

    SHOW_HEAP();

    if (error.code() != DeserializationError::Ok) {
        log_e("Failed to deserialize sync response: %s (length=%d)", error.c_str(), contentLength);
        return 0;
    }

    if (!resp->fromJson(respJson->as<JsonObject>())) {
        log_e("Failed to parse firmware response");
        return 0;
    }

    SHOW_HEAP();

    return httpCode;
}

auto SindarinAPI::downloadReadable(const Readable &readable, FileSystem &fileSystem,
                                   const std::string &path,
                                   const std::function<void(int percentage)> &progressCallback)
    -> bool {
    bool success = false;
    // auto remoteUrl = readable.remoteUrl();
    auto file = readable.file();
    auto remoteUrl = file.fileProxied;
    auto fileSize = file.size;

    // Create the directory if necessary
    // Grab the dir from the path
    auto dir = path.substr(0, path.find_last_of('/'));
    if (!fileSystem.exists(dir)) {
        log_d("Creating directory %s", dir.c_str());
        if (!fileSystem.createDir(dir)) {
            log_e("Could not create %s directory", dir.c_str());
            return false;
        }
    }

    bool gzip = HasSuffix(path, ".gz");
    success = DownloadFile(
        remoteUrl.c_str(), fileSystem, path.c_str(), fileSize,
        [progressCallback](float percentage) { progressCallback(percentage); }, gzip);

    return success;
}

auto SindarinAPI::uploadAndResetVoltageLog(FileSystem &fileSystem) -> int {
    auto localPath = Battery::voltageLogPath();
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
        log_w("HttpClient failed to write fully (writeLength=%d, serializedLength=%zu)",
              writeLength, serializedLength);
    }

    heap_caps_free(serialized);

    client.fetchHeaders();
    int httpCode = client.getStatusCode();

    log_d("Voltage log upload http code: %d", httpCode);

    return httpCode;
}

auto SindarinAPI::uploadCrashReport(SFile &crashFile, const std::string &contentType) -> int {
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
