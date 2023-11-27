#if USE_BLE_SERVER_PROXY

#include "Communications/BLEHttpClient.hpp"

BLEHttpClient::BLEHttpClient(const char *url, const char *contentType,
                             esp_http_client_method_t method)
    : HttpClient(url, contentType, method) {
    url_ = strdup(url);
    client_ = nullptr;
    connected = false;
}

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
