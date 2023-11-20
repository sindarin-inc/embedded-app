#pragma once

#include <NimBLEClient.h>
#include <NimBLEDevice.h>
#include <esp_wifi.h>

#include "BLEAbstractions.hpp"
#include "Communications/BLEManager.hpp"
#include "Misc/StringUtil.hpp"
#include "Storage/PreferencesStore.hpp"
#include "sindarin-debug.h"

class BLEProxy : BLECharacteristicProvider {
public:
    BLEProxy(PreferencesStore *prefs);
    BLEProxy(BLEProxy const &) = delete;

    void configureCharacteristics(BLEService *service) override;

    void
    setWebRequestHandler(std::function<void(const std::string &, const std::string &)> handler);

protected:
    PreferencesStore *prefs_;
    BLECharacteristic *proxyCharacteristic_;

    void writeUrlToCharacteristic(BLECharacteristic *characteristic, const std::string &url);
    void writeBodyToCharacteristic(BLECharacteristic *characteristic, const SpiramString &body);

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
        }
    };
};
