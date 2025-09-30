#include "SindarinAPI.hpp"

#include <utility>

#if CONFIG_USE_BLE_SERVER_PROXY
#include "Communications/BLEHttpClient.hpp"
#include "Communications/BLEManager.hpp"
#include "Communications/BLEProxy.hpp"
#include "Communications/BLESyncService.hpp"
#endif
#include "Communications/UrlEncode.hpp"
#include "Communications/WiFiHttpClient.hpp"
#include "FileDownload.hpp"
#include "Misc/Battery.hpp"
#include "Misc/Defer.hpp"
#include "Misc/Platform.hpp"
#include "Misc/StringUtil.hpp"
#include "Models/Json.hpp"
#include "Models/UserReadable.hpp"
#include "Version.hpp"
#include "sindarin-debug.h"

#define SINDARIN_SERVER_URL "https://api.sindarin.com"
// #define SINDARIN_SERVER_URL "https://staging.sindarin.com"

#define KINDLE_USE_STAGING 0

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
        auto message = StringFormat("HttpClient failed to write (writeLength=%d, bodyLength=%zu)",
                                    writeLength, bodyLength);
        log_w("%s", message.c_str());
        telemetry.logMetric(TelemetryField::METRIC_SYNC_STATUS, "sync-failed",
                            {{"message", message}});
        return ERR_CLIENT_WRITE;
    }

    int contentLength = client->fetchHeaders(cancel);
    if (contentLength <= 0) {
        std::string message = "Failed to fetch headers";
        log_e("%s", message.c_str());
        telemetry.logMetric(TelemetryField::METRIC_SYNC_STATUS, "sync-failed",
                            {{"message", message}});
        return ERR_CONTENT_LENGTH;
    }
    int httpCode = client->getStatusCode();
    if (httpCode != 200) {
        auto message = StringFormat("Failed to check sync: %d", httpCode);
        log_e("%s", message.c_str());
        telemetry.logMetric(TelemetryField::METRIC_SYNC_STATUS, "sync-failed",
                            {{"message", message}});
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
                            {{"message", message}});
        return ERR_JSON_DESERIALIZE;
    }

    SHOW_HEAP();

    if (!resp->fromJson(respJson->as<JsonObject>())) {
        std::string message = "Failed to parse sync response";
        log_e("%s", message.c_str());
        telemetry.logMetric(TelemetryField::METRIC_SYNC_STATUS, "sync-failed",
                            {{"message", message}});
        return ERR_JSON_OBJECT_PARSE;
    }

    log_d("Synced: %d Message: %s", resp->synced, resp->message.c_str());

    SHOW_HEAP();

    return httpCode;
}

auto SindarinAPI::checkFirmware(FirmwareResponse *resp, bool useBle,
                                const std::shared_ptr<CancelFlag> &cancel) -> int {
    // Try HTTPS first
    int httpCode = tryCheckFirmware(resp, false, useBle, cancel);

    // If HTTPS fails, try HTTP
    if (httpCode != 200) {
        httpCode = tryCheckFirmware(resp, true, useBle, cancel);
    }

    return httpCode;
}

auto SindarinAPI::tryCheckFirmware(FirmwareResponse *resp, bool useHttp, bool useBle,
                                   const std::shared_ptr<CancelFlag> &cancel) -> int {
    auto endpoint = StringFormat("%s/firmware/", baseUrl_.c_str());
    if (useHttp) {
        // Replace the https prefix with http
        if (HasPrefix(endpoint.c_str(), "https://")) {
            endpoint = StringFormat("http://%s", endpoint.c_str() + 8);
        }
    }

    auto client = createClient(endpoint, "application/json", HTTP_METHOD_GET, useBle, cancel);
    if (client == nullptr) {
        return ERR_CLIENT_OPEN;
    }

    auto err = setAuth(client);
    if (err != ESP_OK) {
        return ERR_SET_AUTH;
    }

    err = client->open(0);
    if (err != ESP_OK) {
        return ERR_CLIENT_OPEN;
    }
    defer { client->close(); };

    auto contentLength = client->fetchHeaders(cancel);
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
                                   const std::string &publicKey,
                                   const std::function<void(int percentage)> &progressCallback)
    -> int {
    // auto remoteUrl = readable.remoteUrl();
    auto file = readable->file(path);
    auto remoteUrl = file.fileProxied;

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

    std::string requestBody;
    if (!publicKey.empty()) {
        auto doc = std::make_unique<SpiRamJsonDocument>(512);
        auto body = doc->to<JsonObject>();
        body["public_key"] = publicKey;
        ArduinoJson::serializeJson(*doc, requestBody);
        log_d("Request body: %s", requestBody.c_str());
    }
    auto httpCode = DownloadFileToPath(
        remoteUrl.c_str(), fileSystem, telemetry, path.c_str(), 0, cancel, useBle,
        [progressCallback](float percentage) { progressCallback(percentage); }, gzip, requestBody);

    return httpCode;
}

auto SindarinAPI::createClient(const std::string &url, const std::string &contentType,
                               esp_http_client_method_t method, bool useBle,
                               const std::shared_ptr<CancelFlag> &cancel, int timeoutMs)
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
#if CONFIG_USE_WIFI
        // Return an instance of WiFiHttpClient
        return std::make_shared<WiFiHttpClient>(url.c_str(), contentType.c_str(), method, false,
                                                timeoutMs);
#else
        return nullptr;
#endif
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
    client->fetchHeaders(cancel);
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

auto SindarinAPI::getServerTime(time_t *time, bool useBle,
                                const std::shared_ptr<CancelFlag> &cancel) -> int {
    auto endpoint = StringFormat("%s/time/", baseUrl_.c_str());
    auto client = createClient(endpoint, "text/plain", HTTP_METHOD_GET, useBle, cancel);
    if (client == nullptr) {
        return ERR_CLIENT_OPEN;
    }

    auto err = client->open(0);
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
        log_e("Failed to get time: %d", httpCode);
        return httpCode;
    }

    // Read the timestamp directly as a string
    auto buffer = SpiRamArrayPtr<char>(contentLength + 1);
    auto stream = client->makeReadStream();
    size_t totalRead = 0;

    while (totalRead < contentLength) {
        auto read = client->read(buffer.get() + totalRead, contentLength - totalRead);
        if (read <= 0) {
            break;
        }
        totalRead += read;
    }

    if (totalRead != contentLength) {
        log_e("Failed to read time response");
        return ERR_READ_EMPTY;
    }
    buffer[totalRead] = '\0';

    // Convert string to time_t
    *time = strtoll(buffer.get(), nullptr, 10);
    if (*time == 0) {
        log_e("Failed to parse time response: %s", buffer.get());
        return ERR_JSON_DESERIALIZE;
    }

    return httpCode;
}

auto SindarinAPI::askQuestion(const BookQuestionRequest &req, BookQuestionResponse *resp,
                              bool useBle, const std::shared_ptr<CancelFlag> &cancel) -> int {
    auto url = StringFormat("%s/questions/ask/", baseUrl_.c_str());
    log_d("Sending question request to: %s", url.c_str());

    auto client = createClient(url, "application/json", HTTP_METHOD_POST, useBle, cancel, 120000);
    if (client == nullptr) {
        log_e("Failed to create client");
        return ERR_CLIENT_OPEN;
    }

    auto err = setAuth(client);
    if (err != ESP_OK) {
        log_e("Failed to set auth");
        return ERR_SET_AUTH;
    }

    // Serialize request
    auto doc = std::make_shared<SpiRamJsonDocument>(1024);
    JsonObject body = doc->to<JsonObject>();
    req.toJson(body);

    size_t serializedMaxSize = (*doc).memoryUsage() * 2;
    auto reqBody = SpiRamArrayPtr<char>(serializedMaxSize);
    size_t bodyLength = serializeJson(*doc, reqBody.get(), serializedMaxSize);

    err = client->open(bodyLength);
    if (err != ESP_OK) {
        log_e("Failed to open client connection");
        return ERR_CLIENT_OPEN;
    }
    defer { client->close(); };

    int writeLength = client->write(reqBody.get(), bodyLength);
    if (writeLength != bodyLength) {
        log_e("Failed to write request body (wrote %d of %zu bytes)", writeLength, bodyLength);
        return ERR_CLIENT_WRITE;
    }

    int contentLength = client->fetchHeaders(cancel);
    if (contentLength <= 0) {
        log_e("Failed to fetch headers or empty response");
        return ERR_CONTENT_LENGTH;
    }

    int httpCode = client->getStatusCode();
    log_d("Response status code: %d", httpCode);
    if (httpCode != 200) {
        log_e("Request failed with status code: %d", httpCode);
        return httpCode;
    }

    auto stream = client->makeReadStream();
    auto respJson = std::make_unique<SpiRamJsonDocument>(1024 + contentLength * 3);
    auto error = ArduinoJson::deserializeJson(*respJson, *stream);

    if (error.code() != DeserializationError::Ok) {
        log_e("Failed to deserialize response: %s", error.c_str());
        return ERR_JSON_DESERIALIZE;
    }

    if (!resp->fromJson(respJson->as<JsonObject>())) {
        log_e("Failed to parse response JSON into BookQuestionResponse");
        return ERR_JSON_OBJECT_PARSE;
    }

    log_d("Successfully parsed response (success=%d)", resp->success);
    return httpCode;
}

auto SindarinAPI::makeKindleRequest(const std::string &path, KindlePageResponse *resp,
                                    const std::shared_ptr<CancelFlag> &cancelFlag, bool useBle)
    -> int {
    std::string url = StringFormat("%s%s", baseUrl_.c_str(), path.c_str());
#if KINDLE_USE_STAGING
    url += (url.find('?') != std::string::npos) ? "&staging=1" : "?staging=1";
#endif
    log_i("Making Kindle request to URL: %s", url.c_str());

    auto client =
        createClient(url, "application/json", HTTP_METHOD_GET, useBle, cancelFlag, 120000);
    if (!client) {
        log_e("Failed to create HTTP client");
        return ERR_CLIENT_OPEN;
    }

    // Set authentication
    auto err = setAuth(client);
    if (err != ESP_OK) {
        log_e("Failed to set authentication: %d", err);
        return ERR_SET_AUTH;
    }

    // Open the client
    err = client->open();
    if (err != ESP_OK) {
        log_e("Failed to open HTTP client: %d", err);
        return ERR_CLIENT_OPEN;
    }
    defer { client->close(); };

    // Fetch headers - this returns content length, not an error code
    int contentLength = client->fetchHeaders(cancelFlag);
    if (contentLength <= 0) {
        log_e("Failed to fetch HTTP headers or empty response: %d", contentLength);
        return ERR_CONTENT_LENGTH;
    }

    int httpCode = client->getStatusCode();

    // Always try to read the response body, even for error status codes
    // This allows us to get the error message from the server
    auto stream = client->makeReadStream();
    if (!stream) {
        log_e("Failed to create read stream");
        return ERR_CLIENT_READ;
    }

    auto respJson = std::make_unique<SpiRamJsonDocument>(1024 + contentLength * 3);
    auto error = ArduinoJson::deserializeJson(*respJson, *stream);

    if (error.code() != DeserializationError::Ok) {
        log_e("Failed to deserialize Kindle response: %s (length=%d)", error.c_str(),
              contentLength);
        return ERR_JSON_DESERIALIZE;
    }

    if (!resp->fromJson(respJson->as<JsonObject>())) {
        log_e("Failed to parse Kindle page response");
        return ERR_JSON_OBJECT_PARSE;
    }

    return httpCode;
}

auto SindarinAPI::openKindleBook(const SpiramString &title, KindlePageResponse *resp,
                                 const std::shared_ptr<CancelFlag> &cancelFlag, bool useBle)
    -> int {
    if (!resp) {
        assert(false && "Null KindlePageResponse");
    }

    // Validate parameters
    if (title.empty()) {
        log_e("Empty title provided to openKindleBook");
        return ERR_INVALID_PARAM;
    }

    // Properly URL encode the title
    std::string encodedTitle = Urlencode(std::string(title));
    log_i("Encoded title for Kindle API: %s", encodedTitle.c_str());

    // Construct the path with URL-encoded title
    std::string path = "/kindle/open-book?ocr=1&title=" + encodedTitle;
    log_i("Kindle open book path: %s", path.c_str());

    // Make the request (no try-catch since exceptions are disabled)
    int result = makeKindleRequest(path, resp, cancelFlag, useBle);

    // Log the result
    if (result == 200) {
        log_i("Successfully opened Kindle book '%s'", title.c_str());
    } else {
        log_e("Failed to open Kindle book '%s', HTTP code: %d", title.c_str(), result);
    }

    return result;
}

auto SindarinAPI::navigateKindle(const SpiramString &title, KindlePageResponse *resp,
                                 const std::shared_ptr<CancelFlag> &cancelFlag, bool useBle,
                                 int navigateTo, int previewTo, const SpiramString &bookSessionKey)
    -> int {
    std::string encodedTitle = Urlencode(std::string(title));

    std::string path = StringFormat("/kindle/navigate?navigate_to=%d", navigateTo);
    if (previewTo >= 0) {
        path += StringFormat("&preview_to=%d", previewTo);
    }
    path += StringFormat("&title=%s", encodedTitle.c_str());

    std::string encodedSessionKey = Urlencode(std::string(bookSessionKey));
    path += "&book_session_key=" + encodedSessionKey;

    return makeKindleRequest(path, resp, cancelFlag, useBle);
}

auto SindarinAPI::submitLastReadPageChoice(bool gotoLastReadPage, bool useBle,
                                           const std::shared_ptr<CancelFlag> &cancel) -> int {
    std::string path = "/kindle/last-read-page-dialog";

    log_i("Submitting user's choice for last read page dialog: %s",
          gotoLastReadPage ? "sync to latest" : "stay on current page");

    std::string url = StringFormat("%s%s", baseUrl_.c_str(), path.c_str());
#if KINDLE_USE_STAGING
    url += "?staging=1";
#endif
    log_i("Making POST request to URL: %s", url.c_str());

    auto client = createClient(url, "application/json", HTTP_METHOD_POST, useBle, cancel, 120000);
    if (!client) {
        log_e("Failed to create HTTP client");
        return ERR_CLIENT_OPEN;
    }

    auto err = setAuth(client);
    if (err != ESP_OK) {
        log_e("Failed to set authentication: %d", err);
        return ERR_SET_AUTH;
    }

    auto doc = std::make_shared<SpiRamJsonDocument>(128);
    JsonObject body = doc->to<JsonObject>();
    body["goto_last_read_page"] = gotoLastReadPage;

    size_t serializedMaxSize = (*doc).memoryUsage() * 2;
    auto reqBody = SpiRamArrayPtr<char>(serializedMaxSize);
    size_t bodyLength = serializeJson(*doc, reqBody.get(), serializedMaxSize);

    err = client->open(bodyLength);
    if (err != ESP_OK) {
        log_e("Failed to open HTTP client: %d", err);
        return ERR_CLIENT_OPEN;
    }
    defer { client->close(); };

    int writeLength = client->write(reqBody.get(), bodyLength);
    if (writeLength != bodyLength) {
        log_e("Failed to write request body: expected=%zu, actual=%d", bodyLength, writeLength);
        return ERR_CLIENT_WRITE;
    }

    // Fetch headers
    int contentLength = client->fetchHeaders(nullptr);
    if (contentLength <= 0) {
        log_e("Failed to fetch HTTP headers or empty response: %d", contentLength);
        return ERR_CONTENT_LENGTH;
    }

    int httpCode = client->getStatusCode();

    if (httpCode == 200) {
        log_i("Successfully submitted last read page choice");
    } else {
        log_w("Failed to submit last read page choice, HTTP code: %d", httpCode);
    }

    return httpCode;
}

auto SindarinAPI::fetchKindleChaptersWithUuids(const std::string &kindleUuid, int currentPage,
                                               KindleChaptersResponse *resp, bool useBle,
                                               const std::shared_ptr<CancelFlag> &cancel) -> int {
    log_i("Fetching chapters with UUIDs for Kindle book UUID '%s' from page %d", kindleUuid.c_str(),
          currentPage);

    std::string url = StringFormat("%s/kindle/table-of-contents", baseUrl_.c_str());
#if KINDLE_USE_STAGING
    url += "?staging=1";
#endif

    auto client = createClient(url, "application/json", HTTP_METHOD_POST, useBle, cancel, 120000);
    if (!client) {
        log_e("Failed to create HTTP client");
        return ERR_CLIENT_OPEN;
    }

    auto err = setAuth(client);
    if (err != ESP_OK) {
        log_e("Failed to set authentication: %d", err);
        return ERR_SET_AUTH;
    }

    // Build JSON request body with empty chapter_uuid to get chapters with UUIDs
    auto doc = std::make_shared<SpiRamJsonDocument>(512);
    JsonObject body = doc->to<JsonObject>();
    body["kindle_uuid"] = kindleUuid;
    body["chapter_uuid"] = ""; // Empty UUID to get chapter list
    body["page"] = currentPage;

    size_t serializedMaxSize = (*doc).memoryUsage() * 2;
    auto reqBody = SpiRamArrayPtr<char>(serializedMaxSize);
    size_t serializedSize = serializeJson(*doc, reqBody.get(), serializedMaxSize);

    log_d("Request body: %s", reqBody.get());

    err = client->open(serializedSize);
    if (err != ESP_OK) {
        log_e("Failed to open HTTP client: %d", err);
        return ERR_CLIENT_OPEN;
    }
    defer { client->close(); };

    // Write the POST data
    int written = client->write(reqBody.get(), serializedSize);
    if (written != serializedSize) {
        log_e("Failed to write POST data");
        return ERR_CLIENT_WRITE;
    }

    int contentLength = client->fetchHeaders(cancel);
    if (contentLength <= 0) {
        log_e("Failed to fetch headers or no content");
        return ERR_CONTENT_LENGTH;
    }

    auto stream = client->makeReadStream();
    auto respJson = std::make_unique<SpiRamJsonDocument>(1024 + contentLength * 3);
    auto error = ArduinoJson::deserializeJson(*respJson, *stream);

    int httpCode = client->getStatusCode();
    log_i("HTTP response code: %d", httpCode);

    if (httpCode != 200) {
        log_e("HTTP request failed with code %d", httpCode);
        return httpCode;
    }

    if (error.code() != DeserializationError::Ok) {
        log_e("Failed to deserialize response: %s", error.c_str());
        return ERR_JSON_DESERIALIZE;
    }

    if (!resp->fromJson(respJson->as<JsonObject>())) {
        log_e("Failed to parse response JSON into KindleChaptersResponse");
        return ERR_JSON_OBJECT_PARSE;
    }

    log_i("Successfully received chapters with UUIDs response");
    return httpCode;
}

auto SindarinAPI::navigateKindleToChapter(const std::string &kindleUuid,
                                          const std::string &chapterUuid, int currentPage,
                                          KindlePageResponse *resp, bool useBle,
                                          const std::shared_ptr<CancelFlag> &cancel) -> int {
    log_i("Navigating to chapter UUID '%s' for Kindle book UUID '%s' from page %d",
          chapterUuid.c_str(), kindleUuid.c_str(), currentPage);

    std::string url = StringFormat("%s/kindle/table-of-contents", baseUrl_.c_str());
#if KINDLE_USE_STAGING
    url += "?staging=1";
#endif

    auto client = createClient(url, "application/json", HTTP_METHOD_POST, useBle, cancel, 120000);
    if (!client) {
        log_e("Failed to create HTTP client");
        return ERR_CLIENT_OPEN;
    }

    auto err = setAuth(client);
    if (err != ESP_OK) {
        log_e("Failed to set authentication: %d", err);
        return ERR_SET_AUTH;
    }

    // Build JSON request body
    auto doc = std::make_shared<SpiRamJsonDocument>(512);
    JsonObject body = doc->to<JsonObject>();
    body["kindle_uuid"] = kindleUuid;
    body["chapter_uuid"] = chapterUuid;
    body["page"] = currentPage;

    size_t serializedMaxSize = (*doc).memoryUsage() * 2;
    auto reqBody = SpiRamArrayPtr<char>(serializedMaxSize);
    size_t serializedSize = serializeJson(*doc, reqBody.get(), serializedMaxSize);

    log_d("Request body: %s", reqBody.get());

    err = client->open(serializedSize);
    if (err != ESP_OK) {
        log_e("Failed to open HTTP client: %d", err);
        return ERR_CLIENT_OPEN;
    }
    defer { client->close(); };

    // Write the POST data
    int written = client->write(reqBody.get(), serializedSize);
    if (written != serializedSize) {
        log_e("Failed to write POST data");
        return ERR_CLIENT_WRITE;
    }

    int contentLength = client->fetchHeaders(cancel);
    if (contentLength <= 0) {
        log_e("Failed to fetch headers or no content");
        return ERR_CONTENT_LENGTH;
    }

    int httpCode = client->getStatusCode();
    log_i("HTTP response code: %d", httpCode);

    if (httpCode != 200) {
        log_e("HTTP error: %d", httpCode);
        return httpCode;
    }

    // Read and parse response
    auto stream = client->makeReadStream();
    auto respJson = std::make_unique<SpiRamJsonDocument>(1024 + contentLength * 3);
    auto error = ArduinoJson::deserializeJson(*respJson, *stream);
    if (error.code() != DeserializationError::Ok) {
        log_e("Failed to deserialize response: %s", error.c_str());
        return ERR_JSON_DESERIALIZE;
    }

    if (!resp->fromJson(respJson->as<JsonObject>())) {
        log_e("Failed to parse response JSON into KindlePageResponse");
        return ERR_JSON_OBJECT_PARSE;
    }

    log_i("Successfully navigated to chapter (response text length: %zu)", resp->text.size());
    return httpCode;
}
