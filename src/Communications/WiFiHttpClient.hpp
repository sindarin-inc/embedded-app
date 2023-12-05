#pragma once

// Technically we're not using anything Arduino here, but there's a bug in Arduino where it has to
// come first or it breaks compilation
// See: https://github.com/espressif/arduino-esp32/issues/6247
#include <esp_http_client.h>
#include <map>
#include <memory>

#include "Communications/HttpClient.hpp"
#include "Communications/HttpIstream.hpp"
#include "Communications/HttpOstream.hpp"
#include "Misc/GZipStream.hpp"

class WiFiHttpClient : public HttpClient {
public:
    WiFiHttpClient(const char *url, const char *contentType = "",
                   esp_http_client_method_t method = HTTP_METHOD_GET, bool gzip = false);
    WiFiHttpClient(const WiFiHttpClient &) = delete;

    ~WiFiHttpClient() override;

    auto setHeader(const char *key, const char *value) -> esp_err_t override;

    auto getResponseHeader(const char *key, char **value) -> esp_err_t override;

    auto open(int contentLength = 0) -> esp_err_t override;

    // auto perform() -> esp_err_t;

    auto write(const char *body, int length) -> int override;

    auto makeWriteStream() -> std::shared_ptr<HttpOstream<4096>> override;

    auto fetchHeaders() -> int override;

    // auto getContentLength() -> int;

    auto getStatusCode() -> int override;

    auto read(char *buffer, int length) -> int override;

    auto close() -> esp_err_t override;

    auto makeReadStream() -> std::shared_ptr<HttpIstream<4096>> override;

    auto isChunkedResponse() -> bool override;

    auto writeChunkedData(const char *data, int length) -> bool override;

    auto endChunkedData() -> bool override;

    bool connected;

private:
    char *url_;
    std::string userAgent_;
    esp_http_client_handle_t client_;
    std::multimap<std::string, std::string, CaseInsensitive> responseHeaders_;
    bool gzip_;
    GZipStream gz_;
    int gzUncompressedBytesRemaining_{};

    auto writeAvailableCompressedData() -> bool;
};
