#if USE_BLE_SERVER_REMOTE

#include "BLESettings.hpp"

#include <utility>

#include "Misc/Platform.hpp"
#include "sindarin-debug.h"

#define SETTINGS_CHARACTERISTIC_UUID "611f68af-5ffe-4944-9b8f-ef6773326268"

BLESettings::BLESettings(PreferencesStore *prefs) { this->prefs_ = prefs; }

void BLESettings::configureCharacteristics(BLEService *service) {
    BLECharacteristic *characteristic = service->createCharacteristic(
        BLEUUID(SETTINGS_CHARACTERISTIC_UUID),
        NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::NOTIFY);

    characteristic->setCallbacks(new CharacteristicCallbacks(this));
    this->settingsCharacteristic_ = characteristic;
};

void BLESettings::writeDeviceSerialToCharacteristic(BLECharacteristic *characteristic,
                                                    const std::string &key) {
    std::string value = key + "=" + GetSerialNumber();
    characteristic->setValue(value);
    characteristic->notify();
}

void BLESettings::blinkLedAfterConnect(BLECharacteristic *characteristic, const std::string &key) {
    for (int i = 0; i < 10; i++) {
#if !SIMULATOR
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
#endif
    }

    std::string value = key + "=1";
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
