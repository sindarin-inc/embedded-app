#pragma once

#include <map>
#include <memory>

#if CONFIG_USE_BLE_SERVER_PROXY
#include "Communications/BLEImprov.hpp"
#include "Communications/BLEManager.hpp"
#include "Communications/BLEProxy.hpp"
#include "Communications/BLESyncService.hpp"
#include "Inputs/BLERemoteInput.hpp"
#endif

#include "Communications/HttpClient.hpp"
#include "Misc/CancelFlag.hpp"
#include "Misc/Dispatch.hpp"
#include "Misc/StringUtil.hpp"

class BLEHttpClient : public HttpClient {
public:
    BLEHttpClient(std::shared_ptr<BLEManager> bleManager, std::shared_ptr<BLEProxy> bleProxy,
                  std::shared_ptr<BLEImprov> bleImprov, std::shared_ptr<BLERemoteInput> remoteInput,
                  const char *url, const char *contentType = "",
                  esp_http_client_method_t method = HTTP_METHOD_GET,
                  std::shared_ptr<CancelFlag> cancel = nullptr);
    BLEHttpClient(const BLEHttpClient &) = delete;
    ~BLEHttpClient() override;

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

    bool connected{false};

private:
    std::shared_ptr<BLEManager> bleManager_;
    std::shared_ptr<BLEProxy> bleProxy_;
    std::shared_ptr<BLEImprov> bleImprov_;
    std::shared_ptr<BLERemoteInput> remoteInput_;
    std::shared_ptr<CancelFlag> cancel_;
    char *url_;
    int statusCode_{};

    esp_http_client_method_t requestMethod_;
    std::vector<std::pair<SpiramString, SpiramString>> requestHeaders_;
    SpiramString requestBody_;
    SpiramString responseBody_;
    bool responseFetched_{};
};
