#pragma once

#include <map>
#include <memory>

#if CONFIG_USE_BLE_SERVER_PROXY
#include "Communications/BLEImprov.hpp"
#include "Communications/BLEManager.hpp"
#include "Communications/BLEProxy.hpp"
#include "Communications/BLESyncService.hpp"
#endif

#include "Communications/HttpClient.hpp"
#include "Misc/CircBuffer.hpp"
#include "Misc/Dispatch.hpp"
#include "Misc/StringUtil.hpp"

#define MAX_BUFFER_SIZE (1 * 1024 * 1024) // 1MB

typedef enum {
    BLE_HTTP_CLIENT_IDLE,
    BLE_HTTP_CLIENT_FETCHING_RESPONSE_HEADERS,
    BLE_HTTP_CLIENT_FETCHING_RESPONSE_BODY,
    BLE_HTTP_CLIENT_WAITING_FOR_END_OF_REQUEST,
} BLEHttpClientState;

class BLEHttpClient : public HttpClient {
public:
    BLEHttpClient(const char *url, std::shared_ptr<CancelFlag> cancelFlag,
                  const char *contentType = "", esp_http_client_method_t method = HTTP_METHOD_GET);
    BLEHttpClient(const BLEHttpClient &) = delete;
    ~BLEHttpClient() override;

    auto setHeader(const char *key, const char *value) -> esp_err_t override;

    auto open(int contentLength = 0) -> esp_err_t override;

    auto write(const char *body, int length) -> int override;

    auto fetchHeaders(const std::shared_ptr<CancelFlag> &cancel = nullptr) -> int override;

    auto getStatusCode() -> int override;

    auto read(char *buffer, int length) -> int override;

    auto close() -> esp_err_t override;

    auto isChunkedResponse() -> bool override;

    auto writeChunkedData(const char *data, int length) -> bool override;

    auto endChunkedData() -> bool override;

    auto handleWebRequest(const SpiramString &key, const char *body, size_t bodySize) -> bool;

    void cancel();

private:
    std::shared_ptr<BLEManager> bleManager_;
    std::shared_ptr<BLEProxy> bleProxy_;
    int statusCode_{};
    int contentLength_{};
    int readLength_{};

    esp_http_client_method_t requestMethod_;
    std::vector<std::pair<SpiramString, SpiramString>> requestHeaders_;
    SpiramString requestBody_;

    CircBuffer responseBody_;
    BLEHttpClientState state_{BLE_HTTP_CLIENT_IDLE};

    SemaphoreHandle_t dataSemaphore_{};
    std::shared_ptr<CancelFlag> cancel_;
};
