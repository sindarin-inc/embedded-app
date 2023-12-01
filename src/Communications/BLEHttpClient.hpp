#pragma once

// Technically we're not using anything Arduino here, but there's a bug in Arduino where it has to
// come first or it breaks compilation
// See: https://github.com/espressif/arduino-esp32/issues/6247
#include <map>
#include <memory>

#if USE_BLE_SERVER_PROXY
#include "Communications/BLEImprov.hpp"
#include "Communications/BLEManager.hpp"
#include "Communications/BLEProxy.hpp"
#include "Communications/BLESyncService.hpp"
#endif

#include "Communications/HttpClient.hpp"
#include "Misc/Dispatch.hpp"
#include "Misc/StringUtil.hpp"

class BLEHttpClient : public HttpClient {
public:
    BLEHttpClient(const char *url, const char *contentType = "",
                  esp_http_client_method_t method = HTTP_METHOD_GET);
    BLEHttpClient(const BLEHttpClient &) = delete;

    ~BLEHttpClient() override = default;

    auto setHeader(const char *key, const char *value) -> esp_err_t override;

    auto getResponseHeader(const char *key, char **value) -> esp_err_t override;

    auto open(int contentLength = 0) -> esp_err_t override;

    auto write(const char *body, int length) -> int override;

    auto fetchHeaders() -> int override;

    auto getStatusCode() -> int override;

    auto read(char *buffer, int length) -> int override;

    auto close() -> esp_err_t override;

    auto makeReadStream() -> std::shared_ptr<HttpIstream<4096>> override;
    auto makeWriteStream() -> std::shared_ptr<HttpOstream<4096>> override;

    auto isChunkedResponse() -> bool override;

    auto writeChunkedData(const char *data, int length) -> bool override;

    auto endChunkedData() -> bool override;

    auto handleWebRequest(const SpiramString &key, const SpiramString &body) -> bool;

    bool connected;

private:
    char *url_;
    int statusCode_;

    esp_http_client_method_t requestMethod_;
    std::vector<std::pair<SpiramString, SpiramString>> requestHeaders_;
    SpiramString requestBody_;
    SpiramString responseBody_;
    bool responseFetched_;
    std::multimap<std::string, std::string, CaseInsensitive> responseHeaders_;
};
