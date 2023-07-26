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
    auto reqBody = SpiRamArrayPtr<char>(serializedMaxSize);
    size_t bodyLength = serializeJson(*doc, reqBody.get(), serializedMaxSize);

    client.open(bodyLength);
    int writeLength = client.write(reqBody.get(), bodyLength);
    if (writeLength < bodyLength) {
        log_w("HttpClient failed to write (writeLength=%d, bodyLength=%zu)", writeLength,
              bodyLength);
        return 0;
    }

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

    SHOW_HEAP();

    if (!resp->fromJson(respJson->as<JsonObject>())) {
        log_e("Failed to parse sync response");
        return 0;
    }

    SHOW_HEAP();

    log_d("Synced: %d Message: %s", resp->synced, resp->message.c_str());

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

auto SindarinAPI::downloadReadable(const std::shared_ptr<Readable> &readable,
                                   FileSystem &fileSystem, Telemetry &telemetry,
                                   const std::string &path,
                                   const std::function<void(int percentage)> &progressCallback)
    -> bool {
    bool success = false;
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
            return false;
        }
    }

    bool gzip = HasSuffix(path, ".gz");
    success = DownloadFile(
        remoteUrl.c_str(), fileSystem, telemetry, path.c_str(), fileSize,
        [progressCallback](float percentage) { progressCallback(percentage); }, gzip);

    return success;
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
    setAuth(client);
    client.setHeader("X-Serial-Number", GetSerialNumber().c_str());
    client.open(totalSize);
    client.write(header.c_str(), header.size());

    // Write the file data in chunks
    size_t bufferSize = 4096;
    auto buffer = SpiRamArrayPtr<char>(bufferSize);
    int bytesWritten = 0;
    int writeSize = telemetryFile.size();
    while (bytesWritten < writeSize) {
        auto bytesRead = telemetryFile.readBytes(buffer.get(), bufferSize);
        bytesWritten += client.write(buffer.get(), bytesRead);
    }

    // Write the multipart footer
    client.write(footer.c_str(), footer.size());

    // Send request and fetch response
    client.fetchHeaders();
    int httpCode = client.getStatusCode();

    return httpCode;
}

auto SindarinAPI::uploadCrashReport(SFile &crashFile, const std::string &contentType) -> int {
    HttpClient client(SINDARIN_SERVER_URL "/coredumps/", contentType.c_str(), HTTP_METHOD_POST);
    setAuth(client);

    int writeSize = crashFile.size();
    client.open(writeSize);

    size_t bufferSize = 4096;
    auto buffer = SpiRamArrayPtr<char>(bufferSize);

    // NOTE: crashFile.available() never goes down but it seems like it should
    int bytesWritten = 0;
    while (bytesWritten < writeSize) {
        auto bytesRead = crashFile.readBytes(buffer.get(), bufferSize);
        bytesWritten += client.write(buffer.get(), bytesRead);
    }

    client.fetchHeaders();
    int httpCode = client.getStatusCode();

    log_d("uploadCrashReport http code %d", httpCode);

    return httpCode;
}
