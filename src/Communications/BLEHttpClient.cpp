#if USE_BLE_SERVER_PROXY

#include "Communications/BLEHttpClient.hpp"

#include <cstddef>
#include <cstdlib>
#include <esp_timer.h>
#include <utility>

#include "Misc/Defer.hpp"

BLEHttpClient::BLEHttpClient(const char *url, const char *contentType,
                             esp_http_client_method_t method)
    : HttpClient(url, contentType, method), requestMethod_(method), responseBody_(MAX_BUFFER_SIZE),
      dataSemaphore_(xSemaphoreCreateBinary()) {

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
    vSemaphoreDelete(dataSemaphore_);
}

auto BLEHttpClient::setHeader(const char *key, const char *value) -> esp_err_t {
    // Ignore Accept-Encoding header
    if (strcmp(key, "Accept-Encoding") == 0) {
        return ESP_OK;
    }

    requestHeaders_.emplace_back(SpiramString(key), SpiramString(value));

    return ESP_OK;
}

auto BLEHttpClient::write(const char *body, int length) -> int {
    requestBody_.append(body, length);

    return length;
}

auto BLEHttpClient::fetchHeaders(int expectedSize) -> int {
    constexpr auto timeoutMs = 30000; // 30 seconds of advertising to connect to BLE proxy on mobile app
    constexpr auto fetchingTimeoutMs = 10 * 60000; // 10 minutes of downloading per book
    auto logDelayMs = 10000;             // 10 seconds between BLE wait logs
    statusCode_ = 0;
    contentLength_ = 0;
    readLength_ = 0;
    responseBodyFetched_ = false;
    responseBodyFetching_ = false;

    // Now that we're connected and authed to a mobile app, we can send the request

    // Ensure connected to BLE proxy
    this->connected = bleManager_->connectedProxy;
    if (!this->connected) {
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
        auto timedOut = (currentTime - startTime) > (static_cast<long long>(timeoutMs) * 1000);
        auto fetchingTimedOut =
            (currentTime - startTime) > (static_cast<long long>(fetchingTimeoutMs) * 1000);
        if (!responseHeadersFetching_ && timedOut) {
            log_e("Timed out waiting for headers response");
            return -1;
        } else if (responseHeadersFetching_ && fetchingTimedOut) {
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
    char *contentLengthHeader = nullptr;
    auto err = getResponseHeader("Content-Length", &contentLengthHeader);
    if (err == ESP_OK) {
        auto contentLength = strtol(contentLengthHeader, nullptr, 10);
        log_d("Content-Length: %ld", contentLength);
        contentLength_ = contentLength;
        return contentLength;
    }

    log_d("No Content-Length header found");

    return 0;
}

auto BLEHttpClient::handleWebRequest(const SpiramString &key, const char *body, size_t bodySize)
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
        log_d("Got status code: %.*s", bodySize, body);
        statusCode_ = strtol(reinterpret_cast<const char *>(body), nullptr, 10);
    } else if (key == "H") {
        // Header
        // Extract header (e.g. "H=Content-Length:12345")
        // Find the positions of '=' and ':'
        if (bodySize == 0) {
            log_d("Received end of headers");
            responseHeadersFetched_ = true;
            return false;
        }
        responseHeadersFetching_ = true;
        // Find colon and extract key and value
        const char *colonPtr = strchr(body, ':');
        if (colonPtr != nullptr) {
            size_t keyLength = colonPtr - body;
            SpiramString headerKey(body, keyLength);
            // Only save Content-Length
            if (headerKey == "Content-Length") {
                std::string headerValue(colonPtr + 1, bodySize - keyLength - 1);
                // responseHeaders_.emplace_back(headerKey, headerValue);
                responseHeaders_.insert(
                    std::pair<std::string, std::string>(headerKey, headerValue));
            }
        }
        // log_d("Received header: %s=%s", headerKey.c_str(), headerValue.c_str());
    } else if (key == "B") {
        // Body
        // Extract chunk (e.g. "B1/2:chunk")
        // Find the positions of '=', '/' and ':'
        const char *slashPos = strchr(body, '/');
        char *colonPos = strchr(body, ':');

        if (!slashPos || !colonPos || slashPos >= colonPos) {
            log_w("Invalid body format: %s", body);
            return false;
        }

        // Parse the chunk number and total chunks from the correct positions
        int32_t chunkNumber = strtol(body, nullptr, 10);
        int32_t totalChunks = strtol(slashPos + 1, nullptr, 10);

        char *chunkData = colonPos + 1;
        size_t chunkDataSize = bodySize - (chunkData - body);
        responseBody_.push(reinterpret_cast<uint8_t *>(chunkData), chunkDataSize);

        if (chunkNumber == 1 || chunkNumber == totalChunks || chunkNumber % 100 == 0) {
            log_d("Received BLE chunk %" PRIi32 "/%" PRIi32
                  ": %zu bytes, response body buffer size: %zu bytes",
                  chunkNumber, totalChunks, chunkDataSize, responseBody_.size());
            log_d("Bytes remaining to download: %d/%d bytes, %d bytes left", readLength_,
                  contentLength_, contentLength_ - readLength_);
        }

        responseBodyFetching_ = true;

        xSemaphoreGive(dataSemaphore_);
    } else if (key == "E") {
        // End of request
        responseBodyFetched_ = true;

        return true;
    }

    return false;
}

auto BLEHttpClient::getStatusCode() -> int { return statusCode_; }

auto BLEHttpClient::read(char *buffer, int length) -> int {
    if (!buffer || length <= 0) {
        log_w("Invalid arguments %p %d", buffer, length);
        return 0; // Invalid arguments
    }

    // Wait for data to be available if buffer is empty
    if (xSemaphoreTake(dataSemaphore_, pdMS_TO_TICKS(10000)) != pdTRUE) {
        // Timeout occurred, no data available
        log_w("TIMEOUT %p %d", buffer, length);
        return 0;
    }

    // Ensure we only copy as much as available and requested
    int bytesToCopy = std::min(responseBody_.size(), length);
    readLength_ += bytesToCopy;
    if (bytesToCopy != length && bytesToCopy != responseBody_.size()) {
        log_d("Read %d bytes, %d remaining in buffer (%d buffer length). Bytes remaining to "
              "download: %d/%d, "
              "%d left",
              bytesToCopy, responseBody_.size(), length, readLength_, contentLength_,
              contentLength_ - readLength_);
    }
    responseBody_.moveFrom(0, reinterpret_cast<uint8_t *>(buffer), bytesToCopy);

    // If there is still data remaining in the buffer, give the semaphore back
    if (responseBody_.size() > 0) {
        xSemaphoreGive(dataSemaphore_);
    }

    return bytesToCopy;
}

auto BLEHttpClient::open(int contentLength) -> esp_err_t {
    // Register web request handler which receives the response from the mobile app
    bleProxy_->registerWebRequestHandler(
        [this](const SpiramString &key, const char *body, size_t bodySize) -> void {
            bool completed = this->handleWebRequest(key, body, bodySize);
            if (completed) {
                log_d("BLE HTTP request completed");
            }
        });

    bleProxy_->registerSubscriptionStatusCallback([this](bool connected) -> void {
        log_d("BLE HTTP subscription status: %d", connected);
        this->connected = connected;
    });

    return ESP_OK;
}

auto BLEHttpClient::close() -> esp_err_t { return ESP_OK; }

auto BLEHttpClient::isChunkedResponse() -> bool { return false; }

auto BLEHttpClient::writeChunkedData(const char *data, int length) -> bool { return true; }

auto BLEHttpClient::endChunkedData() -> bool { return true; }
#endif
