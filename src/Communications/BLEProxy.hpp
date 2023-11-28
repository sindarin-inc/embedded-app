#pragma once

#include <NimBLEClient.h>
#include <NimBLEDevice.h>
#include <esp_wifi.h>

#include "Communications/BLEAbstractions.hpp"
#include "Misc/StringUtil.hpp"
#include "sindarin-debug.h"

class BLEProxy : BLECharacteristicProvider {
public:
    BLEProxy();
    BLEProxy(BLEProxy const &) = delete;
    virtual ~BLEProxy() = default;

    static auto getInstance() -> std::shared_ptr<BLEProxy>;

    void configureCharacteristics(BLEService *service) override;

    void writeUrl(const SpiramString &url);
    void writeHeader(const SpiramString &header, const SpiramString &value);
    void writeBody(const SpiramString &body);

    void
    setWebRequestHandler(std::function<void(const std::string &, const SpiramString &)> handler);

protected:
    static std::shared_ptr<BLEProxy> instance;
    BLECharacteristic *proxyCharacteristic_;
    std::function<void(const std::string &, const SpiramString &)> webRequestHandler_;

    class CharacteristicCallbacks : public BLECharacteristicCallbacks {
    public:
        BLEProxy *enclosed;

        CharacteristicCallbacks(BLEProxy *enclosure) { enclosed = enclosure; }

        void onWrite(NimBLECharacteristic *characteristic, ble_gap_conn_desc *desc) override {
            std::string value = characteristic->getValue();
            if (value.length() == 0) {
                return;
            }

            log_d("BLEProxy::CharacteristicCallbacks::onWrite: %s", value.c_str());
            // Send the body to the web request handler.
            size_t equalsIndex = value.find('=');
            if (equalsIndex != std::string::npos) {
                std::string key = value.substr(0, equalsIndex);
                SpiramString body = SpiramString(value.substr(equalsIndex + 1));
                enclosed->writeBody(body);
                if (enclosed->webRequestHandler_) {
                    enclosed->webRequestHandler_(key, body);
                }
            }
        }
    };
};
