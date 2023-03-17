#pragma once

// Technically we're not using anything Arduino here, but there's a bug in Arduino where it has to
// come first or it breaks compilation
// See: https://github.com/espressif/arduino-esp32/issues/6247
#include <esp_http_client.h>
#include <memory>

#include "Communications/HttpIstream.hpp"
#include "Communications/HttpOstream.hpp"

class HttpClient {
public:
    HttpClient(const char *url, const char *contentType = "",
               esp_http_client_method_t method = HTTP_METHOD_GET);

    ~HttpClient();

    auto setHeader(const char *key, const char *value) -> esp_err_t;

    auto getHeader(const char *key, char **value) -> esp_err_t;

    auto open(int contentLength = 0) -> esp_err_t;

    // auto perform() -> esp_err_t;

    auto write(const char *body, int length) -> int;

    auto makeWriteStream() -> std::shared_ptr<HttpOstream<4096>>;

    auto fetchHeaders() -> int;

    // auto getContentLength() -> int;

    auto getStatusCode() -> int;

    auto read(char *buffer, int length) -> int;

    auto close() -> esp_err_t;

    auto makeReadStream() -> std::shared_ptr<HttpIstream<4096>>;

    bool connected;

private:
    char *url_;
    std::string userAgent_;
    esp_http_client_handle_t client_;
};
