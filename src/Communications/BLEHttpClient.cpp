#if USE_BLE_SERVER_PROXY

#include "Communications/BLEHttpClient.hpp"

#include <cstddef>
#include <cstdlib>
#include <esp_timer.h>
#include <utility>

#include "Misc/Defer.hpp"

BLEHttpClient::BLEHttpClient(std::shared_ptr<BLEManager> bleManager,
                             std::shared_ptr<BLEProxy> bleProxy,
                             std::shared_ptr<BLEImprov> bleImprov,
                             std::shared_ptr<BLERemoteInput> remoteInput, const char *url,
                             const char *contentType, esp_http_client_method_t method)
    : HttpClient(url, contentType, method), bleManager_(std::move(bleManager)),
      bleProxy_(std::move(bleProxy)), bleImprov_(std::move(bleImprov)),
      remoteInput_(std::move(remoteInput)) {
    url_ = strdup(url);
    connected = false;
    requestBody_ = "";
    requestHeaders_.clear();
    requestMethod_ = method;

    bleProxy_->registerSubscriptionStatusCallback(
        [this](bool connected) { this->connected = connected; });
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

    // Create semaphore to wait for BLE to connect
    SemaphoreHandle_t requestCompleteSemaphore = xSemaphoreCreateBinary();
    if (requestCompleteSemaphore == nullptr) {
        log_e("Failed to create semaphore");
        return -1;
    }

    // If not connected, wait up to 30s to ensure BLE is connected. If not, bail.
    if (!connected) {
        // Wait for BLE to connect
        auto waitForBLEConnect = [this, requestCompleteSemaphore]() {
            while (!connected) {
                log_d("Waiting for BLE to connect...");
                vTaskDelay(1000 / portTICK_PERIOD_MS);
            }
            xSemaphoreGive(requestCompleteSemaphore);
            vTaskDelete(nullptr);
        };
        DispatchQueue::dispatchOneOff(waitForBLEConnect, 4000, 2, 1);

        // Wait for disk write to complete
        auto timeoutMs = 30000;
        if (xSemaphoreTake(requestCompleteSemaphore, pdMS_TO_TICKS(timeoutMs)) != pdTRUE) {
            log_e("Timed out waiting for BLE to connect");
            vSemaphoreDelete(requestCompleteSemaphore);
            return -1;
        }
    }

    // Now that we're connected to a mobile app, we can send the request
    vSemaphoreDelete(requestCompleteSemaphore);
    // Register web request handler which receives the response from the mobile app
    bleProxy_->registerWebRequestHandler([this](const SpiramString &key, const SpiramString &body) {
        return this->handleWebRequest(key, body);
    });

    // Write URL
    SpiramString method = SpiramString(requestMethod_ == HTTP_METHOD_GET ? "GET" : "POST");
    bleProxy_->writeUrl(url_, method);

    // Write headers
    for (auto &header : requestHeaders_) {
        bleProxy_->writeHeader(header.first, header.second);
    }

    // Write body
    bleProxy_->writeBody(requestBody_);

    // Wait for response up to 30s
    auto timeout = 30000;
    auto startTime = esp_timer_get_time();
    while (!responseFetched_) {
        vTaskDelay(100 / portTICK_PERIOD_MS);
        auto currentTime = esp_timer_get_time();
        if (currentTime - startTime > static_cast<long long>(timeout) * 1000) {
            log_e("Timed out waiting for response");
            return -1;
        }
    }

    return statusCode_;
}

auto BLEHttpClient::handleWebRequest(const SpiramString &key, const SpiramString &body) -> bool {
    // If the key contains more than a single character, it's re-requesting a chunk of the body
    if (key.length() > 1) {
        // Re-send the body chunk requested: B123=. chunkNumber is after the first character and
        // before the =.
        int chunkNumber = strtol(key.substr(1, key.length() - 2).c_str(), nullptr, 10);
        bleProxy_->writeBody(requestBody_, chunkNumber);
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
