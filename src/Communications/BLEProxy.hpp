#pragma once

#include <NimBLEClient.h>
#include <NimBLEDevice.h>
#include <StringUtil/StringUtil.hpp>
#include <esp_wifi.h>

#include "Communications/BLEAbstractions.hpp"
#include "Communications/BLEManager.hpp"
#include "Storage/PreferencesStore.hpp"
#include "sindarin-debug.h"

using WebRequestHandler =
    std::function<void(const SpiramString &key, const char *body, size_t bodySize)>;
using SubscriptionStatusCallback = std::function<void(bool connected)>;

class BLEProxy : BLECharacteristicProvider {
public:
    BLEProxy() = default;
    BLEProxy(BLEProxy const &) = delete;
    ~BLEProxy() override = default;

    static auto getInstance(bool create = true) -> std::shared_ptr<BLEProxy>;

    void setPrefs(PreferencesStore *prefs) { prefs_ = prefs; };
    void configureCharacteristics(BLEService *service) override;
    auto getService() -> BLEService * { return proxyService_; };
    void disconnect(uint16_t connId);

    auto countChunks(const SpiramString &body) -> int;

    void writeUrl(const SpiramString &url, const SpiramString &method);
    void writeHeaders(std::vector<std::pair<SpiramString, SpiramString>> &headers,
                      int specificChunkNumber = -1);
    void sendHeaderChunk(const std::pair<SpiramString, SpiramString> &headerPair, int chunkNumber,
                         int numChunks);
    void writeBody(const SpiramString &body, int specificChunkNumber = -1);
    void sendChunk(const SpiramString &body, int chunkNumber, int numChunks);
    auto calculateChecksum(const SpiramString &body) -> SpiramString;

    void writeResponseBody(const SpiramString &key, const SpiramString &body);
    void acknowledgeKey(const SpiramString &key);
    void writeStatusCheck();

    void registerWebRequestHandler(const WebRequestHandler &handler);
    void registerSubscriptionStatusCallback(const SubscriptionStatusCallback &callback);

protected:
    static std::shared_ptr<BLEProxy> instance;
    BLECharacteristic *proxyCharacteristic_{nullptr};
    BLEService *proxyService_{nullptr};
    WebRequestHandler webRequestHandler_{nullptr};
    SubscriptionStatusCallback subscriptionStatusCallback_{nullptr};
    PreferencesStore *prefs_{nullptr};

    class CharacteristicCallbacks : public BLECharacteristicCallbacks {
    public:
        CharacteristicCallbacks() = default;

        void onSubscribe(NimBLECharacteristic *characteristic, NimBLEConnInfo &connInfo,
                         uint16_t subValue) override {
            log_d("BLEProxy::CharacteristicCallbacks::onSubscribe: %d", subValue);

            if (BLEProxy::getInstance()->subscriptionStatusCallback_) {
                bool isConnected = (subValue != 0);
                BLEProxy::getInstance()->subscriptionStatusCallback_(isConnected);
            }
        }

        void onWrite(NimBLECharacteristic *characteristic, NimBLEConnInfo &connInfo) override {
            NimBLEAttValue value = characteristic->getValue();
            const char *valuePtr = value.c_str();
            size_t valueSize = value.size();

            if (valueSize == 0) {
                log_d("BLEProxy::CharacteristicCallbacks::onWrite: Empty value");
                return;
            }

            // log_d("BLEProxy::CharacteristicCallbacks::onWrite: %zu bytes / %.*s", valueSize, 8,
            //       valuePtr);

            // Find the = character
            const char *equalsPos = strchr(valuePtr, '=');
            if (!equalsPos) {
                log_w("BLEProxy::CharacteristicCallbacks::onWrite: Invalid value format: %s",
                      valuePtr);
                return;
            }

            // Extract the key and body
            size_t equalsIndex = equalsPos - valuePtr;
            SpiramString key = SpiramString(std::string(valuePtr, equalsIndex));
            const char *body = valuePtr + equalsIndex + 1;
            size_t bodyLength = valueSize - (body - valuePtr);

            // Send the body to the web request handler
            if (key == "auth") {
                if (body == BLEProxy::getInstance()->prefs_->bleAuthKey.get().value_or("")) {
                    log_i("BLEProxy::CharacteristicCallbacks::onWrite: Authed");
                    BLEManager::getInstance()->markAsAuthed();
                } else {
                    log_w("BLEProxy::CharacteristicCallbacks::onWrite: Auth failed (%s, expected "
                          "%s)",
                          body,
                          BLEProxy::getInstance()->prefs_->bleAuthKey.get().value_or("").c_str());
                }
            } else if (BLEProxy::getInstance()->webRequestHandler_) {
                // Parse B=#/#:value out of valuePtr using valueSize for bounds
                // if (key == "B" && chunkNumber <= 1) {
                //     // Dump valuePtr as hex
                //     int32_t chunkNumber =
                //         strtol(valuePtr + 2, nullptr, 10); // Parses the number after 'B='
                //     log_d(
                //         "BLEProxy::CharacteristicCallbacks::onWrite: %" PRIi32 " %s",
                //         chunkNumber, BytesToHex(reinterpret_cast<const uint8_t *>(valuePtr),
                //         valueSize).c_str());
                // }
                BLEProxy::getInstance()->webRequestHandler_(key, body, bodyLength);
            } else {
                if (key == "E") {
                    log_i("BLEProxy::CharacteristicCallbacks::onWrite: End of request, already "
                          "handled");
                } else {
                    log_w("BLEProxy::CharacteristicCallbacks::onWrite: No web request handler or "
                          "auth, unhandled: %s",
                          key.c_str());
                }
            }
        }
    };

    CharacteristicCallbacks characteristicCallbacks_;
};
