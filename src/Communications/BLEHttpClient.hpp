#pragma once

#include <map>
#include <memory>

#if USE_BLE_SERVER_PROXY
#include "Communications/BLEManager.hpp"
#include "Communications/BLEProxy.hpp"
#include "Communications/BLESyncService.hpp"
#endif

#include "Communications/HttpClient.hpp"
#include "Misc/CancelFlag.hpp"
#include "Misc/CircBuffer.hpp"
#include "Misc/Dispatch.hpp"
#include "Misc/StringUtil.hpp"

#define MAX_BUFFER_SIZE (1 * 1024 * 1024) // 1MB

class BLEHttpClient : public HttpClient {
public:
    BLEHttpClient(const char *url, const char *contentType = "",
                  esp_http_client_method_t method = HTTP_METHOD_GET,
                  std::shared_ptr<CancelFlag> cancel = nullptr);
    BLEHttpClient(const BLEHttpClient &) = delete;
    ~BLEHttpClient() override;

    auto connectBle(std::shared_ptr<CancelFlag> cancel) -> bool;
    auto disconnectBle();

    auto setHeader(const char *key, const char *value) -> esp_err_t override;

    auto getResponseHeader(const char *key, char **value) -> esp_err_t override;

    auto open(int contentLength = 0) -> esp_err_t override;

    auto write(const char *body, int length) -> int override;

    auto fetchHeaders(int expectedSize = 0) -> int override;

    auto getStatusCode() -> int override;

    auto read(char *buffer, int length) -> int override;

    auto close() -> esp_err_t override;

    auto isChunkedResponse() -> bool override;

    auto writeChunkedData(const char *data, int length) -> bool override;

    auto endChunkedData() -> bool override;

    auto handleWebRequest(const SpiramString &key, const char *body, size_t bodySize) -> bool;

private:
    std::shared_ptr<BLEManager> bleManager_;
    std::shared_ptr<BLEProxy> bleProxy_;
    std::shared_ptr<CancelFlag> cancel_;
    char *url_;
    int statusCode_{};

    esp_http_client_method_t requestMethod_;
    std::vector<std::pair<SpiramString, SpiramString>> requestHeaders_;
    SpiramString requestBody_;

    std::vector<std::pair<SpiramString, SpiramString>> responseHeaders_;
    CircBuffer responseBody_;
    bool responseBodyFetched_{};
    bool responseBodyFetching_{};
    bool responseHeadersFetching_{};
    bool responseHeadersFetched_{};

    SemaphoreHandle_t dataSemaphore_{};
};
