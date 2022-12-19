#pragma once

#include <NimBLEDevice.h>
#include <Preferences.h>

#include "BLEAbstractions.hpp"
#include "Communications/BLEManager.hpp"
#include "Inputs/TTRemoteInput.hpp"
#include "Storage/PreferencesStore.hpp"
#include "esp_wifi.h"
#include "sindarin-debug.h"

class BLESettings : BLECharacteristicProvider {

public:
    BLESettings(PreferencesStore *prefs);
    void configureCharacteristics(BLEService *service) override;
    void attachTTRemoteInput(TTRemoteInput *ttRemoteInput);

protected:
    PreferencesStore *prefs_;
    BLECharacteristic *settingsCharacteristic_;
    TTRemoteInput *ttRemoteInput_;
    void writeDeviceSerialToCharacteristic(BLECharacteristic *characteristic,
                                           const std::string &key);
    void writePageTurnerBuildToCharacteristic(BLECharacteristic *characteristic,
                                              const std::string &key);
    void writePageTurnerBatteryToCharacteristic(BLECharacteristic *characteristic,
                                                const std::string &key);
    void disconnectPageTurnerForSeconds(const int seconds);
    void writeWifiSSIDtoCharacteristic(BLECharacteristic *characteristic, const std::string &key);
    void blinkLedAfterConnect(BLECharacteristic *characteristic, const std::string &key);

    class CharacteristicCallbacks : public BLECharacteristicCallbacks {
    public:
        BLESettings *enclosed;

        CharacteristicCallbacks(BLESettings *enclosure) { enclosed = enclosure; }

        void onWrite(BLECharacteristic *characteristic) override {
            std::string value = characteristic->getValue();
            if (value.length() == 0) {
                return;
            }

            // If value contains an '=' character, it's setting a value to a key.
            // Otherwise, get and write the key's value back to the characteristic.
            size_t equalsIndex = value.find('=');
            if (equalsIndex == std::string::npos) {
                if (value == "wifi-ssid") {
                    this->enclosed->writeWifiSSIDtoCharacteristic(characteristic, value);
                } else if (value == "device-serial") {
                    this->enclosed->writeDeviceSerialToCharacteristic(characteristic, value);
                } else if (value == "page-turner-build") {
                    this->enclosed->writePageTurnerBuildToCharacteristic(characteristic, value);
                } else if (value == "page-turner-battery") {
                    this->enclosed->writePageTurnerBatteryToCharacteristic(characteristic, value);
                } else if (value == "blink-led") {
                    this->enclosed->blinkLedAfterConnect(characteristic, value);
                } else {
                    log_w("GATT client asking to read unknown setting: %s", value.c_str());
                }
            } else {
                std::string settingKey = value.substr(0, equalsIndex);
                std::string settingValue = value.substr(equalsIndex + 1);
                log_i("GATT client writing setting: %s=%s", settingKey.c_str(),
                      settingValue.c_str());

                if (settingKey == "pocket-token") {
                    this->enclosed->prefs_->pocketApiToken.set(settingValue);
                } else if (settingKey == "cloud-token") {
                    log_w("Setting cloud access token to %s", settingValue.c_str());
                    this->enclosed->prefs_->cloudToken.set(settingValue);
                } else if (settingKey == "disconnect-pt-sec") {
                    this->enclosed->disconnectPageTurnerForSeconds(std::stoi(settingValue));
                } else {
                    log_w("GATT client asking to write unknown setting: %s=%s", settingKey.c_str(),
                          settingValue.c_str());
                }
            }
        }
    };
};
