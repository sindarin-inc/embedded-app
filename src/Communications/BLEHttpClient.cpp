#if USE_BLE_SERVER_PROXY

#include "Communications/BLEHttpClient.hpp"

#include <cstddef>
#include <cstdlib>
#include <esp_timer.h>
#include <utility>

#include "Misc/CancelFlag.hpp"
#include "Misc/Defer.hpp"

BLEHttpClient::BLEHttpClient(const char *url, const char *contentType,
                             esp_http_client_method_t method, std::shared_ptr<CancelFlag> cancel)
    : HttpClient(url, contentType, method), cancel_(std::move(cancel)), url_(strdup(url)),
      requestMethod_(method), requestBody_("") {

    requestHeaders_.clear();

    bleManager_ = BLEManager::getInstance();
    bleProxy_ = BLEProxy::getInstance();

    if (method != HTTP_METHOD_GET) {
        requestHeaders_.emplace_back(SpiramString("Content-Type"), SpiramString(contentType));
    }
}

BLEHttpClient::~BLEHttpClient() {
    bleProxy_->registerSubscriptionStatusCallback(nullptr);
    bleProxy_->registerWebRequestHandler(nullptr);
}

auto BLEHttpClient::setHeader(const char *key, const char *value) -> esp_err_t {
    requestHeaders_.emplace_back(SpiramString(key), SpiramString(value));

    return ESP_OK;
}

auto BLEHttpClient::write(const char *body, int length) -> int {
    requestBody_.append(body, length);

    return length;
}

auto BLEHttpClient::fetchHeaders(int expectedSize) -> int {
    auto timeoutMs = 60000; // 1 minute of advertising to connect to BLE proxy on mobile app
    auto fetchingTimeoutMs = 10 * 60000; // 10 minutes of downloading per book
    auto logDelayMs = 10000;             // 10 seconds between BLE wait logs
    statusCode_ = 0;
    responseBodyFetched_ = false;
    responseBodyFetching_ = false;
    responseBody_ = SpiramString("");

    // Now that we're connected and authed to a mobile app, we can send the request
    // Register web request handler which receives the response from the mobile app
    bleProxy_->registerWebRequestHandler(
        [this](const SpiramString &key, const std::vector<uint8_t> &body) {
        bool completed = this->handleWebRequest(key, body);
        if (completed) {
            log_d("BLE HTTP request completed");
        }
    });

    // Ensure connected to BLE proxy
    if (!bleManager_->connectedProxy) {
        log_e("Not connected to BLE proxy");
        return -1;
    }

    // Write URL
    SpiramString method = SpiramString(requestMethod_ == HTTP_METHOD_GET ? "GET" : "POST");
    bleProxy_->writeUrl(url_, method);

    // Write headers
    bleProxy_->writeHeaders(requestHeaders_);

    // Write body
    bleProxy_->writeBody(requestBody_);

    // Wait for response up to 30s
    auto startTime = esp_timer_get_time();
    auto lastLogTime = esp_timer_get_time();
    while (!responseHeadersFetched_) {
        auto currentTime = esp_timer_get_time();
        if (!responseHeadersFetching_ &&
            currentTime - startTime > static_cast<long long>(timeoutMs) * 1000) {
            log_e("Timed out waiting for headers response");
            return -1;
        } else if (responseHeadersFetching_ &&
                   currentTime - startTime > static_cast<long long>(fetchingTimeoutMs) * 1000) {
            // Reset the timer if we're still fetching
            log_e("Timed out waiting for headers response");
            return -1;
        }

        if (esp_timer_get_time() - lastLogTime > (static_cast<long long>(logDelayMs) * 1000)) {
            log_d("Waiting for BLE proxy to respond...");
            lastLogTime = esp_timer_get_time();
        }

        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }

    // Extract the Content-Length header
    auto contentLengthHeader =
        std::find_if(responseHeaders_.begin(), responseHeaders_.end(),
                     [](const std::pair<SpiramString, SpiramString> &header) {
        return header.first == "Content-Length";
        });

    if (contentLengthHeader != responseHeaders_.end()) {
        auto contentLength = strtol(contentLengthHeader->second.c_str(), nullptr, 10);
        log_d("Content-Length: %ld", contentLength);
        responseBody_.reserve(contentLength); // Reserve the expected size for the response body
        return contentLength;
    }

    log_d("No Content-Length header found: %zu headers", responseHeaders_.size());
    for (auto &header : responseHeaders_) {
        log_d("Header: %s=%s", header.first.c_str(), header.second.c_str());
    }

    return 0;
}
auto BLEHttpClient::handleWebRequest(const SpiramString &key, const std::vector<uint8_t> &body)
    -> bool {
    // If the key contains more than a single character, it's re-requesting a chunk of the body
    if (key.length() > 1) {
        // Re-send the body chunk requested: B123=. chunkNumber is after the first character and
        // before the =.
        int chunkNumber = strtol(key.substr(1, key.length() - 2).c_str(), nullptr, 10);
        auto type = key.substr(0, 1);
        if (type == "H") {
            // Header
            bleProxy_->writeHeaders(requestHeaders_, chunkNumber);
        } else if (type == "B") {
            // Body
            bleProxy_->writeBody(requestBody_, chunkNumber);
        }
    } else if (key == "S") {
        // Status code
        log_d("Got status code: %.*s", body.size(), body.data());
        statusCode_ = strtol(reinterpret_cast<const char *>(body.data()), nullptr, 10);
    } else if (key == "H") {
        // Header
        // Extract header (e.g. "H=Content-Length:12345")
        // Find the positions of '=' and ':'
        if (body.size() == 0) {
            log_d("Received end of headers");
            responseHeadersFetched_ = true;
            return false;
        }
        responseHeadersFetching_ = true;
        auto headerStr = SpiramString(reinterpret_cast<const char *>(body.data()), body.size());
        auto colonIndex = headerStr.find(':');
        auto headerKey = headerStr.substr(0, colonIndex);
        auto headerValue = headerStr.substr(colonIndex + 1);
        responseHeaders_.emplace_back(headerKey, headerValue);
        // log_d("Received header: %s=%s", headerKey.c_str(), headerValue.c_str());
    } else if (key == "B") {
        // Body
        // Extract chunk (e.g. "B1/2:chunk")
        // Find the positions of '=', '/' and ':'
        auto bodyStr = SpiramString(reinterpret_cast<const char *>(body.data()), body.size());
        auto slashIndex = bodyStr.find('/');
        auto colonIndex = bodyStr.find(':');

        // Parse the chunk number and total chunks from the correct positions
        auto chunkNumber =
            static_cast<int32_t>(strtol(bodyStr.substr(0, slashIndex).c_str(), nullptr, 10));
        auto totalChunks = static_cast<int32_t>(strtol(
            bodyStr.substr(slashIndex + 1, colonIndex - slashIndex - 1).c_str(), nullptr, 10));

        auto chunk = bodyStr.substr(colonIndex + 1);
        responseBody_.append(chunk);
        if (!dataSemaphore_) {
            dataSemaphore_ = xSemaphoreCreateBinary();
        }
        xSemaphoreGive(dataSemaphore_);
        if (chunkNumber == 1 || chunkNumber == totalChunks || chunkNumber % 100 == 0) {
            log_d("Got chunk %" PRIi32 "/%" PRIi32 ": %zu bytes, response body size: %zu bytes",
                  chunkNumber, totalChunks, chunk.length(), responseBody_.length());
        }
        responseBodyFetching_ = true;
    } else if (key == "E") {
        // End of request
        responseBodyFetched_ = true;
        log_d("Got end of request, response body size: %zu bytes", responseBody_.length());
        // log_d("Response body: %s", responseBody_.c_str());
        return true;
    }

    return false;
}

auto BLEHttpClient::getStatusCode() -> int { return statusCode_; }

auto BLEHttpClient::read(char *buffer, int length) -> int {
    if (!buffer || length <= 0) {
        return 0; // Invalid arguments
    }

    if (responseBody_.length() >= length || responseBodyFetched_) {
        // Enough data is available, or all data has been fetched
        int bytesToCopy = std::min(static_cast<int>(responseBody_.length()), length);
        memcpy(buffer, responseBody_.data(), bytesToCopy);
        responseBody_.erase(0, bytesToCopy); // Remove the copied data
        return bytesToCopy;
    }

    // Wait for more data to be available
    // Assuming there is a mechanism to notify or signal when new data arrives, such as a semaphore
    // or event group
    if (!dataSemaphore_) {
        dataSemaphore_ = xSemaphoreCreateBinary();
    }
    if (xSemaphoreTake(dataSemaphore_, pdMS_TO_TICKS(10000)) == pdTRUE) {
        // Data is available after waiting
        int bytesToCopy = std::min(static_cast<int>(responseBody_.length()), length);
        memcpy(buffer, responseBody_.data(), bytesToCopy);
        responseBody_.erase(0, bytesToCopy); // Remove the copied data
        return bytesToCopy;
    } else {
        // Timeout occurred, no data available
        return 0;
    }
}
auto BLEHttpClient::open(int contentLength) -> esp_err_t { return ESP_OK; }

auto BLEHttpClient::getResponseHeader(const char *key, char **value) -> esp_err_t {
    auto header = std::find_if(responseHeaders_.begin(), responseHeaders_.end(),
                               [key](const std::pair<SpiramString, SpiramString> &header) {
        return header.first == key;
    });

    if (header != responseHeaders_.end()) {
        *value = strdup(header->second.c_str());
        return ESP_OK;
    }

    return ESP_FAIL;
}

auto BLEHttpClient::makeWriteStream() -> std::shared_ptr<HttpOstream<4096>> { return nullptr; }

auto BLEHttpClient::close() -> esp_err_t { return ESP_OK; }

auto BLEHttpClient::makeReadStream() -> std::shared_ptr<HttpIstream<4096>> {
    auto timeoutMs = 60000; // 1 minute of advertising to connect to BLE proxy on mobile app
    auto fetchingTimeoutMs = 10 * 60000; // 10 minutes of downloading per book
    auto logDelayMs = 10000;             // 10 seconds between BLE wait logs
    auto startTime = esp_timer_get_time();
    auto lastLogTime = esp_timer_get_time();

    while (!responseBodyFetched_) {
        auto currentTime = esp_timer_get_time();
        if (!responseBodyFetching_ &&
            currentTime - startTime > static_cast<long long>(timeoutMs) * 1000) {
            log_e("Timed out waiting for body response");
            return nullptr;
        } else if (responseBodyFetching_ &&
                   currentTime - startTime > static_cast<long long>(fetchingTimeoutMs) * 1000) {
            // Reset the timer if we're still fetching
            log_e("Timed out waiting for body response");
            return nullptr;
        }

        if (esp_timer_get_time() - lastLogTime > (static_cast<long long>(logDelayMs) * 1000)) {
            log_d("Waiting for BLE proxy to respond...");
            lastLogTime = esp_timer_get_time();
        }

        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }

    // Create an istream using the custom streambuf
    auto stream = std::make_shared<std::istream>(new SpiramStringBuf(responseBody_));

    // Return the istream as an HttpIstream<4096>
    return std::static_pointer_cast<HttpIstream<4096>>(stream);
}

auto BLEHttpClient::isChunkedResponse() -> bool { return false; }

auto BLEHttpClient::writeChunkedData(const char *data, int length) -> bool { return true; }

auto BLEHttpClient::endChunkedData() -> bool { return true; }
#endif
