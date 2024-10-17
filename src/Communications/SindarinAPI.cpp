#include "SindarinAPI.hpp"

#include <utility>

#if CONFIG_USE_BLE_SERVER_PROXY
#include "Communications/BLEHttpClient.hpp"
#include "Communications/BLEManager.hpp"
#include "Communications/BLEProxy.hpp"
#include "Communications/BLESyncService.hpp"
#endif
#include "FileDownload.hpp"
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
        instance = std::make_shared<SindarinAPI>(SINDARIN_SERVER_URL);
    }
    return instance;
}

auto SindarinAPI::isSindarinUrl(const char *url) -> bool {
    return HasPrefix(url, baseUrl_.c_str());
}

auto SindarinAPI::setAuth(const std::shared_ptr<HttpClient> &client) -> esp_err_t {
    if (accessToken_.empty()) {
        log_w("Called setAuth, but no access token is set");
        return ESP_ERR_NOT_FOUND;
    }
    return client->setHeader("Authorization",
                             StringFormat("Tolkien %s", accessToken_.c_str()).c_str());
}

void SindarinAPI::setAccessToken(const std::string &token) { accessToken_ = token; }

auto SindarinAPI::checkSync(CheckSyncRequest &req, CheckSyncResponse *resp, Telemetry &telemetry,
                            bool useBle, const std::shared_ptr<CancelFlag> &cancel) -> int {
    auto url = StringFormat("%s/sync/%ss/", baseUrl_.c_str(),
                            ReadableModelTypeToString(req.modelType).c_str());
    log_d("Checking sync for %s: %s", ReadableModelTypeToString(req.modelType).c_str(),
          url.c_str());
    return checkSync(url, req, resp, telemetry, useBle, cancel);
}

auto SindarinAPI::checkSync(const std::string &url, CheckSyncRequest &req, CheckSyncResponse *resp,
                            Telemetry &telemetry, bool useBle,
                            const std::shared_ptr<CancelFlag> &cancel) -> int {
    auto client = createClient(url, "application/json", HTTP_METHOD_POST, useBle, cancel);
    if (client == nullptr) {
        return ERR_CLIENT_OPEN;
    }

    auto err = setAuth(client);
    if (err != ESP_OK) {
        return ERR_SET_AUTH;
    }

    auto doc = std::make_shared<SpiRamJsonDocument>(
        2048 * std::max(1, static_cast<int>(req.placemarks.size() + req.archives.size() +
                                            req.bookmarks.size() + req.storedUuids.size())));
    JsonObject body = doc->to<JsonObject>();
    ConvertToJson(req, body);

    size_t serializedMaxSize = (*doc).memoryUsage() * 2;
    auto reqBody = SpiRamArrayPtr<char>(serializedMaxSize);
    size_t bodyLength = serializeJson(*doc, reqBody.get(), serializedMaxSize);

    err = client->open(bodyLength);
    if (err != ESP_OK) {
        return ERR_CLIENT_OPEN;
    }
    defer { client->close(); };

    int writeLength = client->write(reqBody.get(), bodyLength);
    if (writeLength != bodyLength) {
        auto message =
            StringFormat("WiFiHttpClient failed to write (writeLength=%d, bodyLength=%zu)",
                         writeLength, bodyLength);
        log_w("%s", message.c_str());
        telemetry.logMetric(TelemetryField::METRIC_SYNC_STATUS, "sync-failed",
                            {{"message", message.c_str()}});
        return ERR_CLIENT_WRITE;
    }

    int contentLength = client->fetchHeaders(0, cancel);
    if (contentLength <= 0) {
        auto message = "Failed to fetch headers";
        log_e("%s", message);
        telemetry.logMetric(TelemetryField::METRIC_SYNC_STATUS, "sync-failed",
                            {{"message", message}});
        return ERR_CONTENT_LENGTH;
    }
    int httpCode = client->getStatusCode();
    if (httpCode != 200) {
        auto message = StringFormat("Failed to check sync: %d", httpCode);
        log_e("%s", message.c_str());
        telemetry.logMetric(TelemetryField::METRIC_SYNC_STATUS, "sync-failed",
                            {{"message", message.c_str()}});
        return httpCode;
    }
    auto stream = client->makeReadStream();

    SHOW_HEAP();

    log_d("Check Sync read stream, contentLength=%d", contentLength);
    auto respJson = std::make_unique<SpiRamJsonDocument>(1024 + contentLength * 3);
    auto error = ArduinoJson::deserializeJson(*respJson, *stream);
    // log_w("Resp JSON: %s", DumpJson(*respJson).c_str());
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

auto SindarinAPI::checkFirmware(FirmwareResponse *resp, bool useHttp) -> int {
    auto endpoint = StringFormat("%s/firmware/", baseUrl_.c_str());
    if (useHttp) {
        // Replace the https prefix with http
        if (HasPrefix(endpoint.c_str(), "https://")) {
            endpoint = StringFormat("http://%s", endpoint.c_str() + 8);
        }
    }
    auto client = createClient(endpoint, "application/json", HTTP_METHOD_GET, false, nullptr);

    auto err = setAuth(client);
    if (err != ESP_OK) {
        return ERR_SET_AUTH;
    }

    err = client->open(0);
    if (err != ESP_OK) {
        return ERR_CLIENT_OPEN;
    }
    defer { client->close(); };

    auto contentLength = client->fetchHeaders();
    if (contentLength <= 0) {
        log_e("Failed to fetch headers");
        return ERR_CONTENT_LENGTH;
    }

    int httpCode = client->getStatusCode();
    if (httpCode != 200) {
        log_e("Failed to check sync: %d", httpCode);
        return httpCode;
    }
    auto stream = client->makeReadStream();

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
                                   const std::shared_ptr<CancelFlag> &cancel, bool useBle,
                                   const std::function<void(int percentage)> &progressCallback)
    -> int {
    // auto remoteUrl = readable.remoteUrl();
    auto file = readable->file(path);
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
    auto httpCode = DownloadFileToPath(
        remoteUrl.c_str(), fileSystem, telemetry, path.c_str(), fileSize, cancel, useBle,
        [progressCallback](float percentage) { progressCallback(percentage); }, gzip);

    return httpCode;
}

auto SindarinAPI::createClient(const std::string &url, const std::string &contentType,
                               esp_http_client_method_t method, bool useBle,
                               const std::shared_ptr<CancelFlag> &cancel)
    -> std::shared_ptr<HttpClient> {
    HEAP_PROFILE_START(createClient);
    defer { HEAP_PROFILE_END(createClient); };
    if (useBle) {
#if CONFIG_USE_BLE_SERVER_PROXY
        return std::make_shared<BLEHttpClient>(url.c_str(), cancel, contentType.c_str(), method);
#else
        return nullptr;
#endif
    } else {
        // Return an instance of WiFiHttpClient
        return std::make_shared<WiFiHttpClient>(url.c_str(), contentType.c_str(), method);
    }
}

auto SindarinAPI::uploadTelemetryFile(SFile &telemetryFile, const std::string &localPath,
                                      FileSystem &fileSystem, bool useBle,
                                      const std::shared_ptr<CancelFlag> &cancel) -> int {
    // Construct the headers and footers for multipart file upload
    auto url = StringFormat("%s/telemetry/", baseUrl_.c_str());
    static std::string boundary = "SIND12345";
    std::string header = "--" + boundary + "\r\n" +
                         R"(Content-Disposition: form-data; name="file"; filename=")" + localPath +
                         "\"\r\n" + "Content-Type: text/plain\r\n\r\n";
    std::string footer = "\r\n--" + boundary + "--\r\n";

    // Create client with multipart headers and write the header
    int totalSize = header.size() + telemetryFile.size() + footer.size();
    auto contentType = "multipart/form-data; boundary=" + boundary;
    auto client = createClient(url, contentType, HTTP_METHOD_POST, useBle, cancel);
    if (client == nullptr) {
        return ERR_CLIENT_OPEN;
    }

    auto err = setAuth(client);
    if (err != ESP_OK) {
        return ERR_SET_AUTH;
    }

    err = client->setHeader("X-Serial-Number", GetSerialNumber().c_str());
    if (err != ESP_OK) {
        return ERR_SET_HEADER;
    }

    err = client->open(totalSize);
    if (err != ESP_OK) {
        return ERR_CLIENT_OPEN;
    }
    defer { client->close(); };

    auto bytesWritten = client->write(header.c_str(), header.size());
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
        auto bytesWritten = client->write(buffer.get(), bytesRead);
        if (bytesWritten != bytesRead) {
            log_w("client->write failed (bytesWritten == %d)", bytesWritten);
            if (!client->connected) {
                log_w("Connection closed, bailing out");
                return ERR_CLIENT_DISCONNECTED;
            }
        }
        totalBytesWritten += bytesWritten;
    }

    // Write the multipart footer
    bytesWritten = client->write(footer.c_str(), footer.size());
    if (bytesWritten < footer.size()) {
        log_w("client->write failed (%d < %zu)", bytesWritten, footer.size());
        return ERR_CLIENT_WRITE;
    }

    // Send request and fetch response
    client->fetchHeaders(0, cancel);
    auto httpCode = client->getStatusCode();
    log_d("Telemetry upload http code: %d", httpCode);

    // Download the body
    auto stream = client->makeReadStream();
    auto body = std::make_unique<SpiRamJsonDocument>(1024);
    auto error = ArduinoJson::deserializeJson(*body, *stream);
    if (error.code() != DeserializationError::Ok) {
        log_e("Failed to deserialize telemetry response: %s", error.c_str());
        return ERR_JSON_DESERIALIZE;
    }
    log_d("Telemetry response: %s", DumpJson(*body).c_str());

    return httpCode;
}

auto SindarinAPI::uploadCrashReport(SFile &crashFile, const std::string &contentType) -> int {
    auto client = createClient(StringFormat("%s/coredumps/", baseUrl_.c_str()), contentType,
                               HTTP_METHOD_POST, false, nullptr);

    auto err = setAuth(client);
    if (err != ESP_OK) {
        return ERR_SET_AUTH;
    }

    int writeSize = crashFile.size();
    err = client->open(writeSize);
    if (err != ESP_OK) {
        return ERR_CLIENT_OPEN;
    }
    defer { client->close(); };

    size_t bufferSize = 4096;
    auto buffer = SpiRamArrayPtr<char>(bufferSize);

    int totalBytesWritten = 0;
    while (totalBytesWritten < writeSize) {
        auto bytesRead = crashFile.readBytes(buffer.get(), bufferSize);
        auto bytesWritten = client->write(buffer.get(), bytesRead);
        if (bytesWritten != bytesRead) {
            log_w("client.write failed (bytesWritten == %d)", bytesWritten);
            if (!client->connected) {
                log_w("Connection closed, bailing out");
                return ERR_CLIENT_DISCONNECTED;
            }
        }
        totalBytesWritten += bytesWritten;
    }

    client->fetchHeaders();
    return client->getStatusCode();
}
