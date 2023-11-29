#if USE_BLE_SERVER_PROXY

#include "Communications/BLEHttpClient.hpp"

#include <esp_timer.h>
#include <utility>

#include "Misc/Defer.hpp"

BLEHttpClient::BLEHttpClient(const char *url, const char *contentType,
                             esp_http_client_method_t method)
    : HttpClient(url, contentType, method) {
    url_ = strdup(url);
    client_ = nullptr;
    connected = false;
    requestBody_ = "";
    requestHeaders_.clear();
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
    int statusCode = 0;
    auto bleProxy = BLEProxy::getInstance();
    // If not connected, using FreeRTOS, xSemaphoreTake, and DispatchQueue::dispatchOneOff, wait up
    // to 30s to ensure BLE is connected. If not, bail.
    if (!connected) {
        // Wait for BLE to connect

        SemaphoreHandle_t requestCompleteSemaphore = xSemaphoreCreateBinary();
        defer { vSemaphoreDelete(requestCompleteSemaphore); };
        auto waitForBLEConnect = [this]() {
            while (!connected) {
                log_d("Waiting for BLE to connect...");
                vTaskDelay(1000 / portTICK_PERIOD_MS);
            }
        };

        DispatchQueue::dispatchOneOff(
            [waitForBLEConnect, requestCompleteSemaphore]() {
            waitForBLEConnect();
            xSemaphoreGive(requestCompleteSemaphore);
            UBaseType_t watermark = uxTaskGetStackHighWaterMark(NULL);
            log_d("BLE sync task stack high watermark: %d", watermark);
            },
            4000, 2, 1);

        // Wait for disk write to complete
        auto timeout = 30000;
        if (xSemaphoreTake(requestCompleteSemaphore, timeout / portTICK_PERIOD_MS) != pdTRUE) {
            log_e("Timed out waiting for BLE to connect");
            return -1;
        }
    }

    // Write URL
    bleProxy->writeUrl(url_);

    // Write headers
    for (auto &header : requestHeaders_) {
        bleProxy->writeHeader(header.first, header.second);
    }

    // Write body
    bleProxy->writeBody(requestBody_);

    // Dispatch queue to advertise sync service

    // Dispatch queue to wait for response
    DispatchQueue::dispatchOneOff(
        [this, &statusCode]() { auto bleProxy = BLEProxy::getInstance(); }, 4000, 2, 1);

    return statusCode;
}

auto BLEHttpClient::getStatusCode() -> int { return 0; }

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
