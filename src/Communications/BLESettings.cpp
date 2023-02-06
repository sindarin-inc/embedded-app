#if USE_BLE_SERVER_REMOTE

#include "BLESettings.hpp"

#include <utility>

#include "Misc/Dispatch.hpp"
#include "Misc/Platform.hpp"
#include "Misc/StringUtil.hpp"
#include "WiFi.hpp"
#include "sindarin-debug.h"

#define SETTINGS_CHARACTERISTIC_UUID "611f68af-5ffe-4944-9b8f-ef6773326268"

BLESettings::BLESettings(PreferencesStore *prefs) { this->prefs_ = prefs; }

void BLESettings::attachTTRemoteInput(TTRemoteInput *ttRemoteInput) {
    this->ttRemoteInput_ = ttRemoteInput;
}

void BLESettings::configureCharacteristics(BLEService *service) {
    BLECharacteristic *characteristic = service->createCharacteristic(
        BLEUUID(SETTINGS_CHARACTERISTIC_UUID),
        NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::NOTIFY);

    characteristic->setCallbacks(new CharacteristicCallbacks(this));
    this->settingsCharacteristic_ = characteristic;
};

void BLESettings::writeDeviceSerialToCharacteristic(BLECharacteristic *characteristic,
                                                    const std::string &key) {
    std::string value = StringFormat("%s=%s", key.c_str(), GetSerialNumber().c_str());
    log_i("GATT client reading serial number: %s", value.c_str());
    characteristic->setValue(value);
    characteristic->notify();
}

void BLESettings::writePageTurnerBuildToCharacteristic(BLECharacteristic *characteristic,
                                                       const std::string &key) {
    auto lastPageTurnerBuild = this->prefs_->lastPageTurnerBuild.get();
    std::string value = StringFormat("%s=%s", key.c_str(),
                                     this->prefs_->lastPageTurnerBuild.get().value_or("").c_str());
    log_i("GATT client reading page turner build: %s", value.c_str());
    characteristic->setValue(value);
    characteristic->notify();
}

void BLESettings::writePageTurnerBatteryToCharacteristic(BLECharacteristic *characteristic,
                                                         const std::string &key) {
    std::string value =
        StringFormat("%s=%d", key.c_str(), this->prefs_->lastPageTurnerBattery.get().value_or(0));
    log_i("GATT client reading page turner battery: %s", value.c_str());
    characteristic->setValue(value);
    characteristic->notify();
}

void BLESettings::disconnectPageTurnerForSeconds(const int seconds) {
    log_i("GATT client disconnecting page turner for %d seconds", seconds);

    ttRemoteInput_->disconnectTemporarily(seconds);
}

void BLESettings::blinkLedAfterConnect(BLECharacteristic *characteristic, const std::string &key) {
    for (int i = 0; i < 10; i++) {
        // Flash all the LEDs
        digitalWrite(RED_LED, LED_ON);
        vTaskDelay(200 / portTICK_PERIOD_MS);
        digitalWrite(RED_LED, LED_OFF);
        digitalWrite(GREEN_LED, LED_ON);
        vTaskDelay(200 / portTICK_PERIOD_MS);
        digitalWrite(GREEN_LED, LED_OFF);
#ifdef BLUE_LED
        digitalWrite(BLUE_LED, LED_ON);
        vTaskDelay(200 / portTICK_PERIOD_MS);
        digitalWrite(BLUE_LED, LED_OFF);
#endif
    }

    std::string value = StringFormat("%s=1", key.c_str());
    characteristic->setValue(value);
    characteristic->notify();
}

void BLESettings::writeWifiSSIDtoCharacteristic(BLECharacteristic *characteristic,
                                                const std::string &key) {
    auto queue = DispatchGetCore0BackgroundQueue();
    queue->dispatch([key, characteristic] {
        // Check if wifi is already connected, and if so, don't disconnect at the end of this
        // function
        bool wifiConnected = NetworkConnected();
        wifi_config_t conf;

        if (GetWiFiCredentials(&conf) && strlen((const char *)conf.sta.ssid) > 0) {
            std::string value = StringFormat("%s=%s", key.c_str(), (const char *)conf.sta.ssid);
            characteristic->setValue(value);

            log_i("GATT client reading wifi ssid, initializing wifi, sharing ssid: %s %zu/%zu",
                  value.c_str(), value.length(), sizeof(value.c_str()));
        } else {
            std::string value = StringFormat("%s=", key.c_str());
            characteristic->setValue(value);
            log_i("GATT client reading wifi ssid, no wifi credentials, sharing blank ssid: %s",
                  value.c_str());
        }

        characteristic->notify();

        // Turn wifi back off if we weren't already using it
        if (!wifiConnected) {
            DisconnectWifi();
        }
    });
}

#endif
