#if USE_BLE_SERVER_REMOTE

#include "BLESettings.hpp"

#include <driver/gpio.h>
#include <utility>

#include "Misc/Dispatch.hpp"
#include "Misc/Platform.hpp"
#include "Misc/StringUtil.hpp"
#include "Version.hpp"
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

void BLESettings::writeFirmwareVersionToCharacteristic(BLECharacteristic *characteristic,
                                                       const std::string &key) {
    std::string value = StringFormat("%s=%s", key.c_str(), kVersionString);
    log_i("GATT client reading firmware version: %s", value.c_str());
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
    std::string value = StringFormat("%s=%d", key.c_str(), PreferencesStore::lastPageTurnerBattery);
    log_i("GATT client reading page turner battery: %s", value.c_str());
    characteristic->setValue(value);
    characteristic->notify();
}

void BLESettings::disconnectPageTurnerForSeconds(const int seconds) {
    log_i("GATT client disconnecting page turner for %d seconds", seconds);

    ttRemoteInput_->disconnectTemporarily(seconds);
}

void BLESettings::blinkLedAfterConnect(BLECharacteristic *characteristic, const std::string &key) {
    // TODO: Fully take over the LED for a while
    gpio_hold_dis((gpio_num_t)RED_LED);
    gpio_hold_dis((gpio_num_t)GREEN_LED);
    gpio_hold_dis((gpio_num_t)BLUE_LED);
    for (int i = 0; i < 10; i++) {
        // Flash all the LEDs
        DigitalWrite(RED_LED, LED_ON);
        vTaskDelay(200 / portTICK_PERIOD_MS);
        DigitalWrite(RED_LED, LED_OFF);
        DigitalWrite(GREEN_LED, LED_ON);
        vTaskDelay(200 / portTICK_PERIOD_MS);
        DigitalWrite(GREEN_LED, LED_OFF);
#ifdef BLUE_LED
        DigitalWrite(BLUE_LED, LED_ON);
        vTaskDelay(200 / portTICK_PERIOD_MS);
        DigitalWrite(BLUE_LED, LED_OFF);
#endif
    }

    std::string value = StringFormat("%s=1", key.c_str());
    characteristic->setValue(value);
    characteristic->notify();
}

void BLESettings::unpairPageTurner(BLECharacteristic *characteristic, const std::string &key) {
    log_i("GATT client unpairing page turner");

    ttRemoteInput_->unpairAndDisconnect();

    std::string value = StringFormat("%s=1", key.c_str());
    characteristic->setValue(value);
    characteristic->notify();
}

void BLESettings::writeWifiSSIDtoCharacteristic(BLECharacteristic *characteristic,
                                                const std::string &key) {
    characteristic->setValue(WiFi::getInstance().getFirstWiFiSSID());
    characteristic->notify();
}

#endif
