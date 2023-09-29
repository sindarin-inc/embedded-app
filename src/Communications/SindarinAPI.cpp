#include "SindarinAPI.hpp"

#include <utility>

#include "FileDownload.hpp"
#include "GitVersion.hpp"
#include "Misc/Battery.hpp"
#include "Misc/Defer.hpp"
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

auto SindarinAPI::setAuth(HttpClient &client) -> esp_err_t {
    if (accessToken_.empty()) {
        log_w("Called setAuth, but no access token is set");
        return ESP_ERR_NOT_FOUND;
    }
    return client.setHeader("Authorization",
                            StringFormat("Tolkien %s", accessToken_.c_str()).c_str());
}

void SindarinAPI::setAccessToken(const std::string &token) { accessToken_ = token; }

auto SindarinAPI::checkSync(CheckSyncRequest &req, CheckSyncResponse *resp, Telemetry &telemetry)
    -> int {
    auto url = StringFormat(SINDARIN_SERVER_URL "/sync/%ss/",
                            ReadableModelTypeToString(req.modelType).c_str());
    log_d("Checking sync for %s: %s", ReadableModelTypeToString(req.modelType).c_str(),
          url.c_str());
    return checkSync(url, req, resp, telemetry);
}

auto SindarinAPI::checkSync(const std::string &url, CheckSyncRequest &req, CheckSyncResponse *resp,
                            Telemetry &telemetry) -> int {
    HttpClient client(url.c_str(), "application/json", HTTP_METHOD_POST);
    auto err = setAuth(client);
    if (err != ESP_OK) {
        return ERR_SET_AUTH;
    }

    auto doc = std::make_shared<SpiRamJsonDocument>(
        2048 * std::max(1, (int)(req.placemarks.size() + req.archives.size() +
                                 req.bookmarks.size() + req.storedUuids.size())));
    JsonObject body = doc->to<JsonObject>();
    ConvertToJson(req, body);

    size_t serializedMaxSize = (*doc).memoryUsage() * 2;
    auto reqBody = SpiRamArrayPtr<char>(serializedMaxSize);
    size_t bodyLength = serializeJson(*doc, reqBody.get(), serializedMaxSize);

    err = client.open(bodyLength);
    if (err != ESP_OK) {
        return ERR_CLIENT_OPEN;
    }
    defer { client.close(); };

    int writeLength = client.write(reqBody.get(), bodyLength);
    if (writeLength != bodyLength) {
        auto message = StringFormat("HttpClient failed to write (writeLength=%d, bodyLength=%zu)",
                                    writeLength, bodyLength);
        log_w("%s", message.c_str());
        telemetry.logMetric(TelemetryField::METRIC_SYNC_STATUS, "sync-failed",
                            {{"message", message.c_str()}});
        return ERR_CLIENT_WRITE;
    }

    int contentLength = client.fetchHeaders();
    if (contentLength <= 0) {
        auto message = "Failed to fetch headers";
        log_e("%s", message);
        telemetry.logMetric(TelemetryField::METRIC_SYNC_STATUS, "sync-failed",
                            {{"message", message}});
        return ERR_CONTENT_LENGTH;
    }
    int httpCode = client.getStatusCode();
    if (httpCode != 200) {
        auto message = StringFormat("Failed to check sync: %d", httpCode);
        log_e("%s", message.c_str());
        telemetry.logMetric(TelemetryField::METRIC_SYNC_STATUS, "sync-failed",
                            {{"message", message.c_str()}});
        return httpCode;
    }
    auto stream = client.makeReadStream();

    SHOW_HEAP();

    auto respJson = std::make_unique<SpiRamJsonDocument>(1024 + contentLength * 3);
    auto error = ArduinoJson::deserializeJson(*respJson, *stream);

    SHOW_HEAP();

    if (error.code() != DeserializationError::Ok) {
        auto message = StringFormat("Failed to deserialize sync response: %s (length=%d)",
                                    error.c_str(), contentLength);
        log_e("%s", message.c_str());
        telemetry.logMetric(TelemetryField::METRIC_SYNC_STATUS, "sync-failed",
                            {{"message", message.c_str()}});
        return ERR_JSON_DESERIALIZE;
    }

    SHOW_HEAP();

    if (!resp->fromJson(respJson->as<JsonObject>())) {
        auto message = "Failed to parse sync response";
        log_e("%s", message);
        telemetry.logMetric(TelemetryField::METRIC_SYNC_STATUS, "sync-failed",
                            {{"message", message}});
        return ERR_JSON_OBJECT_PARSE;
    }

    log_d("Synced: %d Message: %s", resp->synced, resp->message.c_str());

    SHOW_HEAP();

    return httpCode;
}

auto SindarinAPI::checkFirmware(FirmwareResponse *resp) -> int {
    HttpClient client(SINDARIN_SERVER_URL "/firmware/");

    auto err = setAuth(client);
    if (err != ESP_OK) {
        return ERR_SET_AUTH;
    }

    err = client.open(0);
    if (err != ESP_OK) {
        return ERR_CLIENT_OPEN;
    }
    defer { client.close(); };

    auto contentLength = client.fetchHeaders();
    if (contentLength <= 0) {
        log_e("Failed to fetch headers");
        return ERR_CONTENT_LENGTH;
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
        return ERR_JSON_DESERIALIZE;
    }

    if (!resp->fromJson(respJson->as<JsonObject>())) {
        log_e("Failed to parse firmware response");
        return ERR_JSON_OBJECT_PARSE;
    }

    SHOW_HEAP();

    return httpCode;
}

auto SindarinAPI::downloadReadable(const std::shared_ptr<Readable> &readable,
                                   FileSystem &fileSystem, Telemetry &telemetry,
                                   const std::string &path,
                                   const std::function<void(int percentage)> &progressCallback)
    -> int {
    // auto remoteUrl = readable.remoteUrl();
    auto file = readable->file();
    auto remoteUrl = file.fileProxied;
    auto fileSize = file.size;

    // Create the directory if necessary
    // Grab the dir from the path
    auto dir = path.substr(0, path.find_last_of('/'));
    if (!fileSystem.exists(dir)) {
        log_d("Creating directory %s", dir.c_str());
        if (!fileSystem.createDir(dir)) {
            log_e("Could not create %s directory", dir.c_str());
            return ERR_CREATE_DIR;
        }
    }

    bool gzip = HasSuffix(path, ".gz");
    auto httpCode = DownloadFile(
        remoteUrl.c_str(), fileSystem, telemetry, path.c_str(), fileSize,
        [progressCallback](float percentage) { progressCallback(percentage); }, gzip);

    return httpCode;
}

auto SindarinAPI::uploadTelemetryFile(SFile &telemetryFile, const std::string &localPath,
                                      FileSystem &fileSystem) -> int {
    // Construct the headers and footers for multipart file upload
    static std::string boundary = "SIND12345";
    std::string header = "--" + boundary + "\r\n" +
                         R"(Content-Disposition: form-data; name="file"; filename=")" + localPath +
                         "\"\r\n" + "Content-Type: text/plain\r\n\r\n";
    std::string footer = "\r\n--" + boundary + "--\r\n";

    // Create client with multipart headers and write the header
    int totalSize = header.size() + telemetryFile.size() + footer.size();
    auto contentType = "multipart/form-data; boundary=" + boundary;

    HttpClient client(SINDARIN_SERVER_URL "/telemetry/", contentType.c_str(), HTTP_METHOD_POST);

    auto err = setAuth(client);
    if (err != ESP_OK) {
        return ERR_SET_AUTH;
    }

    err = client.setHeader("X-Serial-Number", GetSerialNumber().c_str());
    if (err != ESP_OK) {
        return ERR_SET_HEADER;
    }

    err = client.open(totalSize);
    if (err != ESP_OK) {
        return ERR_CLIENT_OPEN;
    }
    defer { client.close(); };

    auto bytesWritten = client.write(header.c_str(), header.size());
    if (bytesWritten != header.size()) {
        log_e("Telemetry multipart write failure (%d < %zu)", bytesWritten, header.size());
        return ERR_CLIENT_WRITE;
    }

    // Write the file data in chunks
    size_t bufferSize = 4096;
    auto buffer = SpiRamArrayPtr<char>(bufferSize);
    int totalBytesWritten = 0;
    int writeSize = telemetryFile.size();
    while (totalBytesWritten < writeSize) {
        auto bytesRead = telemetryFile.readBytes(buffer.get(), bufferSize);
        auto bytesWritten = client.write(buffer.get(), bytesRead);
        if (bytesWritten != bytesRead) {
            log_w("client.write failed (bytesWritten == %d)", bytesWritten);
            if (!client.connected) {
                log_w("Connection closed, bailing out");
                return ERR_CLIENT_DISCONNECTED;
            }
        }
        totalBytesWritten += bytesWritten;
    }

    // Write the multipart footer
    bytesWritten = client.write(footer.c_str(), footer.size());
    if (bytesWritten < footer.size()) {
        log_w("client.write failed (%d < %zu)", bytesWritten, footer.size());
        return ERR_CLIENT_WRITE;
    }

    // Send request and fetch response
    client.fetchHeaders();
    return client.getStatusCode();
}

auto SindarinAPI::uploadCrashReport(SFile &crashFile, const std::string &contentType) -> int {
    HttpClient client(SINDARIN_SERVER_URL "/coredumps/", contentType.c_str(), HTTP_METHOD_POST,
                      true);
    auto err = setAuth(client);
    if (err != ESP_OK) {
        return ERR_SET_AUTH;
    }

    int writeSize = crashFile.size();
    err = client.open(writeSize);
    if (err != ESP_OK) {
        return ERR_CLIENT_OPEN;
    }
    defer { client.close(); };

    size_t bufferSize = 4096;
    auto buffer = SpiRamArrayPtr<char>(bufferSize);

    int totalBytesWritten = 0;
    while (totalBytesWritten < writeSize) {
        auto bytesRead = crashFile.readBytes(buffer.get(), bufferSize);
        auto bytesWritten = client.write(buffer.get(), bytesRead);
        if (bytesWritten != bytesRead) {
            log_w("client.write failed (bytesWritten == %d)", bytesWritten);
            if (!client.connected) {
                log_w("Connection closed, bailing out");
                return ERR_CLIENT_DISCONNECTED;
            }
        }
        totalBytesWritten += bytesWritten;
    }

    client.fetchHeaders();
    return client.getStatusCode();
}
