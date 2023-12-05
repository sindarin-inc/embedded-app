#pragma once

#include <NimBLEClient.h>
#include <NimBLEDevice.h>
#include <esp_wifi.h>

#include "Communications/BLEAbstractions.hpp"
#include "Misc/StringUtil.hpp"
#include "sindarin-debug.h"

using WebRequestHandler = std::function<void(const SpiramString &, const SpiramString &)>;
using SubscriptionStatusCallback = std::function<void(bool)>;

class BLEProxy : BLECharacteristicProvider {
public:
    BLEProxy() = default;
    BLEProxy(BLEProxy const &) = delete;
    ~BLEProxy() override = default;

    static auto getInstance() -> std::shared_ptr<BLEProxy>;

    void configureCharacteristics(BLEService *service) override;
    auto getService() -> BLEService * { return proxyService_; };

    auto countChunks(const SpiramString &body) -> int;

    void writeUrl(const SpiramString &url, const SpiramString &method);
    void writeHeader(const SpiramString &header, const SpiramString &value);
    void writeBody(const SpiramString &body);
    void writeBody(const SpiramString &body, int chunkNumber);

    void writeResponseBody(const SpiramString &key, const SpiramString &body);

    void registerWebRequestHandler(const WebRequestHandler &handler);
    void registerSubscriptionStatusCallback(const SubscriptionStatusCallback &callback);

protected:
    static std::shared_ptr<BLEProxy> instance;
    BLECharacteristic *proxyCharacteristic_{nullptr};
    BLEService *proxyService_{nullptr};
    WebRequestHandler webRequestHandler_{nullptr};
    SubscriptionStatusCallback subscriptionStatusCallback_{nullptr};

    class CharacteristicCallbacks : public BLECharacteristicCallbacks {
    public:
        CharacteristicCallbacks() = default;

        void onSubscribe(NimBLECharacteristic *characteristic, ble_gap_conn_desc *desc,
                         uint16_t subValue) override {
            log_d("BLEProxy::CharacteristicCallbacks::onSubscribe: %d", subValue);

            if (BLEProxy::getInstance()->subscriptionStatusCallback_) {
                bool isConnected = (subValue != 0);
                BLEProxy::getInstance()->subscriptionStatusCallback_(isConnected);
            }
        }

        void onWrite(NimBLECharacteristic *characteristic, ble_gap_conn_desc *desc) override {
            std::string value = characteristic->getValue();
            if (value.length() == 0) {
                return;
            }

            log_d("BLEProxy::CharacteristicCallbacks::onWrite: %s", value.c_str());
            // Send the body to the web request handler.
            size_t equalsIndex = value.find('=');
            if (equalsIndex != std::string::npos) {
                SpiramString key = SpiramString(value.substr(0, equalsIndex));
                SpiramString body = SpiramString(value.substr(equalsIndex + 1));
                if (BLEProxy::getInstance()->webRequestHandler_) {
                    BLEProxy::getInstance()->webRequestHandler_(key, body);
                }
            }
        }
    };

    CharacteristicCallbacks characteristicCallbacks_;
};
