#if USE_BLE_SERVER_REMOTE

#include "BLESettings.hpp"

#include <utility>

#include "Misc/Platform.hpp"
#include "Misc/StringUtil.hpp"
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
        StringFormat("%s=%d", key.c_str(), this->prefs_->lastPageTurnerBattery.get().value());
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
                                                std::string key) {
    wifi_ap_record_t info;

    if (!esp_wifi_sta_get_ap_info(&info)) {
        key.append("=");
        key.append(reinterpret_cast<char *>(info.ssid));
        characteristic->setValue(key);

        log_i("Connected to wifi, sharing ssid: %s %zu/%zu", key.c_str(), key.length(),
              sizeof(key.c_str()));
    } else {
        key.append("=");
        characteristic->setValue(key);
        log_i("Not connected to wifi, share blank ssid: %s", key.c_str());
    }

    characteristic->notify();
}

#endif
