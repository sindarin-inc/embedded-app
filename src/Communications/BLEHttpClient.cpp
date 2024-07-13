#if CONFIG_USE_BLE_SERVER_PROXY

#include "Communications/BLEHttpClient.hpp"

#include <cstddef>
#include <cstdlib>
#include <esp_timer.h>
#include <utility>

#include "Misc/CancelFlag.hpp"
#include "Misc/Defer.hpp"

BLEHttpClient::BLEHttpClient(std::shared_ptr<BLEManager> bleManager,
                             std::shared_ptr<BLEProxy> bleProxy,
                             std::shared_ptr<BLEImprov> bleImprov,
                             std::shared_ptr<BLERemoteInput> remoteInput, const char *url,
                             const char *contentType, esp_http_client_method_t method,
                             std::shared_ptr<CancelFlag> cancel)
    : HttpClient(url, contentType, method), bleManager_(std::move(bleManager)),
      bleProxy_(std::move(bleProxy)), bleImprov_(std::move(bleImprov)),
      remoteInput_(std::move(remoteInput)), cancel_(std::move(cancel)), url_(strdup(url)),
      requestMethod_(method), requestBody_("") {

    requestHeaders_.clear();

    bleProxy_->registerSubscriptionStatusCallback(
        [this](bool connected) { this->connected = connected; });

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

auto BLEHttpClient::fetchHeaders() -> int {
    auto timeoutMs = 60000;  // 1 minute of advertising to connect to BLE proxy on mobile app
    auto logDelayMs = 10000; // 10 seconds between BLE wait logs
    statusCode_ = 0;
    responseFetched_ = false;
    responseBody_ = "";

    // Configure BLE sync service and advertise it
    bleManager_->stopAdvertising();
    bleManager_->disableService(remoteInput_->getService());
    bleManager_->disableService(bleImprov_->getService());
    bleManager_->enableService(bleProxy_->getService());
    bleManager_->startAdvertisingIfNecessary();
    // Re-enable existing services after we're done
    defer {
        bleManager_->stopAdvertising();
        bleManager_->disableService(bleProxy_->getService());
        bleManager_->enableService(remoteInput_->getService());
        bleManager_->enableService(bleImprov_->getService());
        bleManager_->startAdvertisingIfNecessary();
    };

    // If not connected, wait up to 30s to ensure BLE is connected. If not, bail.
    auto startTime = esp_timer_get_time();
    auto lastLogTime = esp_timer_get_time();
    while (!connected) {
        if (esp_timer_get_time() - startTime > static_cast<long long>(timeoutMs) * 1000) {
            log_e("Timed out waiting for BLE proxy to connect");
            return -1;
        }

        if (esp_timer_get_time() - lastLogTime > (static_cast<long long>(logDelayMs) * 1000)) {
            log_d("Waiting for BLE proxy to connect...");
            lastLogTime = esp_timer_get_time();
        }

        vTaskDelay(1000 / portTICK_PERIOD_MS);

        if (CancelFlag::isCancelled(cancel_)) {
            log_d("Cancelled BLE connection");
            return -1;
        }
    }

    startTime = esp_timer_get_time();
    lastLogTime = esp_timer_get_time();
    while (!BLEManager::getInstance()->isAuthed()) {
        if (esp_timer_get_time() - startTime > static_cast<long long>(timeoutMs) * 1000) {
            log_e("Timed out waiting for BLE proxy to auth");
            return -1;
        }

        if (esp_timer_get_time() - lastLogTime > (static_cast<long long>(logDelayMs) * 1000)) {
            log_d("Waiting for BLE proxy to auth...");
            lastLogTime = esp_timer_get_time();
        }

        vTaskDelay(1000 / portTICK_PERIOD_MS);

        if (CancelFlag::isCancelled(cancel_)) {
            log_d("Cancelled BLE connection while waiting for auth");
            return -1;
        }
    }

    // Now that we're connected and authed to a mobile app, we can send the request
    // Register web request handler which receives the response from the mobile app
    bleProxy_->registerWebRequestHandler([this](const SpiramString &key, const SpiramString &body) {
        bool completed = this->handleWebRequest(key, body);
        if (completed) {
            // Once telemetry is complete, disconnect from the mobile app
            bleProxy_->disconnect(bleManager_->getConnId());
        }
    });

    // Write URL
    SpiramString method = SpiramString(requestMethod_ == HTTP_METHOD_GET ? "GET" : "POST");
    bleProxy_->writeUrl(url_, method);

    // Write headers
    bleProxy_->writeHeaders(requestHeaders_);

    // Write body
    bleProxy_->writeBody(requestBody_);

    // Wait for response up to 30s
    startTime = esp_timer_get_time();
    lastLogTime = esp_timer_get_time();
    while (!responseFetched_) {
        auto currentTime = esp_timer_get_time();
        if (currentTime - startTime > static_cast<long long>(timeoutMs) * 1000) {
            log_e("Timed out waiting for response");
            return -1;
        }

        if (esp_timer_get_time() - lastLogTime > (static_cast<long long>(logDelayMs) * 1000)) {
            log_d("Waiting for BLE proxy to respond...");
            lastLogTime = esp_timer_get_time();
        }

        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }

    return statusCode_;
}

auto BLEHttpClient::handleWebRequest(const SpiramString &key, const SpiramString &body) -> bool {
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
        log_d("Got status code: %s", body.c_str());
        statusCode_ = strtol(body.c_str(), nullptr, 10);
    } else if (key == "B") {
        // Body
        // Extract chunk (e.g. "B1/2:chunk")
        auto slashIndex = body.find('/');
        auto colonIndex = body.find(':');
        auto chunkNumber = strtol(body.substr(1, slashIndex - 1).c_str(), nullptr, 10);
        auto totalChunks =
            strtol(body.substr(slashIndex + 1, colonIndex - slashIndex - 1).c_str(), nullptr, 10);
        auto chunk = body.substr(colonIndex + 1);
        responseBody_.append(chunk);
        log_d("Got chunk %ld/%ld: %s", chunkNumber, totalChunks, chunk.c_str());
    } else if (key == "E") {
        // End of request
        responseFetched_ = true;
        log_d("Got end of request");
        return true;
    }

    return false;
}

auto BLEHttpClient::getStatusCode() -> int { return statusCode_; }

auto BLEHttpClient::read(char *buffer, int length) -> int { return 0; }

auto BLEHttpClient::open(int contentLength) -> esp_err_t { return ESP_OK; }

auto BLEHttpClient::getResponseHeader(const char *key, char **value) -> esp_err_t { return ESP_OK; }

auto BLEHttpClient::makeWriteStream() -> std::shared_ptr<HttpOstream<4096>> { return nullptr; }

auto BLEHttpClient::close() -> esp_err_t { return ESP_OK; }

auto BLEHttpClient::makeReadStream() -> std::shared_ptr<HttpIstream<4096>> { return nullptr; }

auto BLEHttpClient::isChunkedResponse() -> bool { return false; }

auto BLEHttpClient::writeChunkedData(const char *data, int length) -> bool { return true; }

auto BLEHttpClient::endChunkedData() -> bool { return true; }
#endif
