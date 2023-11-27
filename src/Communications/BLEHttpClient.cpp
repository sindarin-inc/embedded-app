#if USE_BLE_SERVER_PROXY

#include "Communications/BLEHttpClient.hpp"

auto BLEHttpClient::setHeader(const char *key, const char *value) -> esp_err_t {
    requestHeaders_.emplace_back(SpiramString(key), SpiramString(value));

    return ESP_OK;
}

auto BLEHttpClient::write(const char *body, int length) -> int {
    requestBody_ = SpiramString(body);

    return length;
}

auto BLEHttpClient::fetchHeaders() -> int {
    int statusCode = 0;
    auto bleProxy = BLEProxy::getInstance();

    // Write URL
    bleProxy->writeUrl(url_);

    // Write headers
    for (auto &header : requestHeaders_) {
        bleProxy->writeHeader(header.first, header.second);
    }

    // Write body
    bleProxy->writeBody(requestBody_);

    return statusCode;
}

#endif
