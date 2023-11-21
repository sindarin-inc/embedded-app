#pragma once

// Technically we're not using anything Arduino here, but there's a bug in Arduino where it has to
// come first or it breaks compilation
// See: https://github.com/espressif/arduino-esp32/issues/6247
#include <esp_http_client.h>
#include <map>
#include <memory>

#include "Communications/HttpClient.hpp"

class BLEHttpClient {
public:
    BLEHttpClient(const char *url, const char *contentType = "",
                  esp_http_client_method_t method = HTTP_METHOD_GET, bool gzip = false);
    BLEHttpClient(const BLEHttpClient &) = delete;

    ~BLEHttpClient();

    auto setHeader(const char *key, const char *value) -> esp_err_t;

    auto getResponseHeader(const char *key, char **value) -> esp_err_t;

    auto open(int contentLength = 0) -> esp_err_t;

    auto write(const char *body, int length) -> int;

    auto makeWriteStream() -> std::shared_ptr<HttpOstream<4096>>;

    auto fetchHeaders() -> int;

    auto getStatusCode() -> int;

    auto read(char *buffer, int length) -> int;

    auto close() -> esp_err_t;

    bool connected;

private:
    char *url_;
    std::string userAgent_;
    esp_http_client_handle_t client_;
    std::multimap<std::string, std::string, CaseInsensitive> responseHeaders_;
};
