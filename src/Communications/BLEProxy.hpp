#pragma once

#include <NimBLEClient.h>
#include <NimBLEDevice.h>
#include <esp_wifi.h>

#include "Communications/BLEAbstractions.hpp"
#include "Communications/BLEManager.hpp"
#include "Misc/StringUtil.hpp"
#include "Storage/PreferencesStore.hpp"
#include "sindarin-debug.h"

using WebRequestHandler = std::function<void(const SpiramString &, const SpiramString &)>;
using SubscriptionStatusCallback = std::function<void(bool)>;

class BLEProxy : BLECharacteristicProvider {
public:
    BLEProxy() = default;
    BLEProxy(BLEProxy const &) = delete;
    ~BLEProxy() override = default;

    static auto getInstance() -> std::shared_ptr<BLEProxy>;

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
            std::string_view valueStr(reinterpret_cast<const char *>(value.c_str()),
                                      value.length());
            size_t valueSize = value.size();

            if (valueSize == 0) {
                log_d("BLEProxy::CharacteristicCallbacks::onWrite: Empty value");
                return;
            }

            log_d("BLEProxy::CharacteristicCallbacks::onWrite: %s", value.c_str());
            // Send the body to the web request handler.
            size_t equalsIndex = valueStr.find('=');
            if (equalsIndex != std::string::npos) {
                SpiramString key = SpiramString(valueStr.substr(0, equalsIndex));
                SpiramString body = SpiramString(valueStr.substr(equalsIndex + 1));
                if (key == "auth") {
                    if (body == BLEProxy::getInstance()->prefs_->bleAuthKey.get()) {
                        log_i("BLEProxy::CharacteristicCallbacks::onWrite: Authed");
                        BLEManager::getInstance()->markAsAuthed();
                    } else {
                        log_w(
                            "BLEProxy::CharacteristicCallbacks::onWrite: Auth failed (%s, "
                            "expected %s)",
                            body.c_str(),
                            BLEProxy::getInstance()->prefs_->bleAuthKey.get().value_or("").c_str());
                    }
                } else if (BLEProxy::getInstance()->webRequestHandler_) {
                    BLEProxy::getInstance()->webRequestHandler_(key, body);
                }
            }
        }
    };

    CharacteristicCallbacks characteristicCallbacks_;
};
