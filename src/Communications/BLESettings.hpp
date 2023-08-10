#pragma once

#include <NimBLEClient.h>
#include <NimBLEDevice.h>

#include "BLEAbstractions.hpp"
#include "Communications/BLEManager.hpp"
#include "Inputs/TTRemoteInput.hpp"
#include "Misc/StringUtil.hpp"
#include "Storage/PreferencesStore.hpp"
#include "esp_wifi.h"
#include "sindarin-debug.h"

class BLESettings : BLECharacteristicProvider {

public:
    BLESettings(PreferencesStore *prefs);
    BLESettings(BLESettings const &) = delete;
    void configureCharacteristics(BLEService *service) override;
    void attachTTRemoteInput(TTRemoteInput *ttRemoteInput);

protected:
    PreferencesStore *prefs_;
    BLECharacteristic *settingsCharacteristic_;
    TTRemoteInput *ttRemoteInput_;
    void writeDeviceSerialToCharacteristic(BLECharacteristic *characteristic,
                                           const std::string &key);
    void writeFirmwareVersionToCharacteristic(BLECharacteristic *characteristic,
                                              const std::string &key);
    void writePageTurnerBuildToCharacteristic(BLECharacteristic *characteristic,
                                              const std::string &key);
    void writePageTurnerBatteryToCharacteristic(BLECharacteristic *characteristic,
                                                const std::string &key);
    void disconnectPageTurnerForSeconds(const int seconds);
    void unpairPageTurner(BLECharacteristic *characteristic, const std::string &key);
    void writeWifiSSIDtoCharacteristic(BLECharacteristic *characteristic, const std::string &key);
    void blinkLedAfterConnect(BLECharacteristic *characteristic, const std::string &key);

    class CharacteristicCallbacks : public BLECharacteristicCallbacks {
    public:
        BLESettings *enclosed;

        CharacteristicCallbacks(BLESettings *enclosure) { enclosed = enclosure; }

        void onWrite(NimBLECharacteristic *characteristic, ble_gap_conn_desc *desc) override {
            std::string value = characteristic->getValue();
            if (value.length() == 0) {
                return;
            }

            // If value contains an '=' character, it's setting a value to a key.
            // Otherwise, get and write the key's value back to the characteristic.
            size_t equalsIndex = value.find('=');
            if (equalsIndex == std::string::npos) {
                // Public settings
                if (value == "device-serial") {
                    this->enclosed->writeDeviceSerialToCharacteristic(characteristic, value);
                } else if (value == "firmware-version") {
                    this->enclosed->writeFirmwareVersionToCharacteristic(characteristic, value);
                } else if (value == "page-turner-build") {
                    this->enclosed->writePageTurnerBuildToCharacteristic(characteristic, value);
                } else if (value == "page-turner-battery") {
                    this->enclosed->writePageTurnerBatteryToCharacteristic(characteristic, value);
                } else if (value == "blink-led") {
                    this->enclosed->blinkLedAfterConnect(characteristic, value);
                } else if (value == "pair") {
                    characteristic->setValue(StringFormat(
                        "pair=%s", enclosed->prefs_->bleAdvertisingKey.get().value_or("").c_str()));
                    characteristic->notify();
                } else if (!BLEManager::getInstance()->checkAuthed(desc)) {
                    log_w("GATT client asking to read setting while unauthed: %s", value.c_str());
                }
                // Authenticated-only settings
                else if (value == "wifi-ssid") {
                    this->enclosed->writeWifiSSIDtoCharacteristic(characteristic, value);
                } else {
                    log_w("GATT client asking to read unknown setting: %s", value.c_str());
                }
            } else {
                std::string settingKey = value.substr(0, equalsIndex);
                std::string settingValue = value.substr(equalsIndex + 1);
                log_i("GATT client writing setting: %s=%s", settingKey.c_str(),
                      settingValue.c_str());
                // Public settings
                if (settingKey == "blink-led") {
                    this->enclosed->blinkLedAfterConnect(characteristic, value);
                } else if (settingKey == "auth") {
                    // If we don't have auth key set, set it
                    if (!enclosed->prefs_->bleAuthKey.get().has_value()) {
                        log_i("Locking auth key to %s", settingValue.c_str());
                        enclosed->prefs_->bleAuthKey.set(settingValue);
                        BLEManager::getInstance()->markAsAuthed();
                    }
                    // If we're already authed, allow changing the key
                    else if (BLEManager::getInstance()->isAuthed()) {
                        log_i("Changing auth key from %s to %s",
                              enclosed->prefs_->bleAuthKey.get().value_or("").c_str(),
                              settingValue.c_str());
                        enclosed->prefs_->bleAuthKey.set(settingValue);
                    }
                    // If we're not authed but this is the correct key
                    else if (settingValue == enclosed->prefs_->bleAuthKey.get()) {
                        BLEManager::getInstance()->markAsAuthed();
                        log_i("GATT client authenticated");
                    } else {
                        log_i("GATT client bad auth received '%s', disconnecting",
                              settingValue.c_str());
                        characteristic->getService()->getServer()->disconnect(desc->conn_handle);
                    }
                    // Unset the characteristic so it can't be read by unauthed users
                    characteristic->setValue("");
                } else if (!BLEManager::getInstance()->checkAuthed(desc)) {
                    log_w("GATT client trying to write setting while unauthed: %s=%s",
                          settingKey.c_str(), settingValue.c_str());
                }
                // Authenticated-only settings
                else if (settingKey == "pair") {
                    enclosed->prefs_->bleAdvertisingKey.set(settingValue);
                    BLEManager::getInstance()->setManfacturerData(settingValue);
                } else if (settingKey == "cloud-token") {
                    log_w("Setting cloud access token to %s", settingValue.c_str());
                    this->enclosed->prefs_->cloudToken.set(settingValue);
                } else if (settingKey == "disconnect-pt-sec") {
                    this->enclosed->disconnectPageTurnerForSeconds(std::stoi(settingValue));
                } else if (settingKey == "unpair-page-turner") {
                    this->enclosed->unpairPageTurner(characteristic, settingKey);
                } else {
                    log_w("GATT client asking to write unknown setting: %s=%s", settingKey.c_str(),
                          settingValue.c_str());
                }
            }
        }
    };
};
