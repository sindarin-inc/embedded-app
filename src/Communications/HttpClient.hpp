#pragma once

// Technically we're not using anything Arduino here, but there's a bug in Arduino where it has to
// come first or it breaks compilation
// See: https://github.com/espressif/arduino-esp32/issues/6247
#include <esp_http_client.h>
#include <map>
#include <memory>

#include "Communications/HttpIstream.hpp"
#include "Communications/HttpOstream.hpp"
#include "Misc/GZipStream.hpp"

struct CaseInsensitive {
    auto operator()(const std::string &s1, const std::string &s2) const -> bool {
        return std::lexicographical_compare(s1.begin(), s1.end(), s2.begin(), s2.end(),
                                            [](unsigned char c1, unsigned char c2) {
            return ::tolower(c1) < ::tolower(c2);
        });
    }
};

class HttpClient {
public:
    HttpClient(const char *url, const char *contentType = "",
               esp_http_client_method_t method = HTTP_METHOD_GET, bool gzip = false);
    HttpClient(const HttpClient &) = delete;

    ~HttpClient();

    auto setHeader(const char *key, const char *value) -> esp_err_t;

    auto getResponseHeader(const char *key, char **value) -> esp_err_t;

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

    auto isChunkedResponse() -> bool;

    auto writeChunkedData(const char *data, int length) -> bool;

    auto endChunkedData() -> bool;

    bool connected;

private:
    char *url_;
    std::string userAgent_;
    esp_http_client_handle_t client_;
    std::multimap<std::string, std::string, CaseInsensitive> responseHeaders_;
    bool gzip_;
    GZipStream gz_;
    int gzUncompressedBytesRemaining_;

    auto writeAvailableCompressedData() -> bool;
};
