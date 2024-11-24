#pragma once

#include <esp_http_client.h>
#include <map>
#include <memory>

#include "Communications/HttpIstream.hpp"
#include "Communications/HttpOstream.hpp"
#include "Misc/CancelFlag.hpp"
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

    virtual ~HttpClient();

    virtual auto setHeader(const char *key, const char *value) -> esp_err_t = 0;

    virtual auto getResponseHeader(const char *key, char **value) -> esp_err_t {
        auto it = responseHeaders_.find(key);
        if (it == responseHeaders_.end()) {
            return ESP_ERR_NOT_FOUND;
        }
        *value = const_cast<char *>(it->second.c_str());
        return ESP_OK;
    }

    virtual auto open(int contentLength = 0) -> esp_err_t = 0;

    // auto perform() -> esp_err_t;

    virtual auto write(const char *body, int length) -> int = 0;

    virtual auto makeWriteStream() -> std::shared_ptr<HttpOstream<4096>> {
        return std::make_shared<HttpOstream<4096>>(
            [this](char *buffer, size_t size) -> size_t { return write(buffer, size); });
    }

    virtual auto fetchHeaders(const std::shared_ptr<CancelFlag> &cancel = nullptr) -> int = 0;

    // auto getContentLength() -> int;

    virtual auto getStatusCode() -> int = 0;

    virtual auto read(char *buffer, int length) -> int = 0;

    virtual auto close() -> esp_err_t = 0;

    virtual auto makeReadStream() -> std::shared_ptr<HttpIstream<4096>> {
        return std::make_shared<HttpIstream<4096>>(
            [this](char *buffer, size_t size) -> size_t { return read(buffer, size); });
    }

    virtual auto isChunkedResponse() -> bool = 0;

    virtual auto writeChunkedData(const char *data, int length) -> bool = 0;

    virtual auto endChunkedData() -> bool = 0;

    bool connected{};

protected:
    char *url_;
    std::multimap<std::string, std::string, CaseInsensitive> responseHeaders_;
};
