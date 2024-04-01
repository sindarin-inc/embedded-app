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
    responseFetched_ = false;
    responseFetching_ = false;
    responseBody_ = SpiramString("");
    if (expectedSize) {
        responseBody_.reserve(expectedSize); // Reserve 4MB for the response body
    }

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
    while (!responseFetched_) {
        auto currentTime = esp_timer_get_time();
        if (!responseFetching_ &&
            currentTime - startTime > static_cast<long long>(timeoutMs) * 1000) {
            log_e("Timed out waiting for response");
            return -1;
        } else if (responseFetching_ &&
                   currentTime - startTime > static_cast<long long>(fetchingTimeoutMs) * 1000) {
            // Reset the timer if we're still fetching
            log_e("Timed out waiting for response");
            return -1;
        }

        if (esp_timer_get_time() - lastLogTime > (static_cast<long long>(logDelayMs) * 1000)) {
            log_d("Waiting for BLE proxy to respond...");
            lastLogTime = esp_timer_get_time();
        }

        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }

    return responseBody_.length();
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
        if (chunkNumber == 1 || chunkNumber == totalChunks || chunkNumber % 100 == 0) {
            log_d("Got chunk %" PRIi32 "/%" PRIi32 ": %zu bytes, response body size: %zu bytes",
                  chunkNumber, totalChunks, chunk.length(), responseBody_.length());
        }
        responseFetching_ = true;
    } else if (key == "E") {
        // End of request
        responseFetched_ = true;
        log_d("Got end of request, response body size: %zu bytes", responseBody_.length());
        // log_d("Response body: %s", responseBody_.c_str());
        return true;
    }

    return false;
}

auto BLEHttpClient::getStatusCode() -> int { return statusCode_; }

auto BLEHttpClient::read(char *buffer, int length) -> int {
    // Copy the response body to the buffer
    auto bytesRead = std::min(length, static_cast<int>(responseBody_.length()));
    memcpy(buffer, responseBody_.c_str(), bytesRead);

    // Remove the bytes read from the response body
    responseBody_ = responseBody_.substr(bytesRead);

    return bytesRead;
}

auto BLEHttpClient::open(int contentLength) -> esp_err_t { return ESP_OK; }

auto BLEHttpClient::getResponseHeader(const char *key, char **value) -> esp_err_t { return ESP_OK; }

auto BLEHttpClient::makeWriteStream() -> std::shared_ptr<HttpOstream<4096>> { return nullptr; }

auto BLEHttpClient::close() -> esp_err_t { return ESP_OK; }

auto BLEHttpClient::makeReadStream() -> std::shared_ptr<HttpIstream<4096>> {
    // Create an istream using the custom streambuf
    auto stream = std::make_shared<std::istream>(new SpiramStringBuf(responseBody_));

    // Return the istream as an HttpIstream<4096>
    return std::static_pointer_cast<HttpIstream<4096>>(stream);
}

auto BLEHttpClient::isChunkedResponse() -> bool { return false; }

auto BLEHttpClient::writeChunkedData(const char *data, int length) -> bool { return true; }

auto BLEHttpClient::endChunkedData() -> bool { return true; }
#endif
