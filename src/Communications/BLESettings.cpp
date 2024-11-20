#if CONFIG_USE_BLE_SERVER_REMOTE

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

BLESettings::BLESettings(PreferencesStore *prefs) : prefs_(prefs) {}

#if CONFIG_USE_TURN_TOUCH_REMOTE
void BLESettings::attachTTRemoteInput(TTRemoteInput *ttRemoteInput) {
    this->ttRemoteInput_ = ttRemoteInput;
}
#endif

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
    std::string value = StringFormat("%s=%s", key.c_str(), SemanticVersionString());
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

void BLESettings::disconnectPageTurnerForSeconds(int seconds) {
    log_i("GATT client disconnecting page turner for %d seconds", seconds);
#if CONFIG_USE_TURN_TOUCH_REMOTE
    ttRemoteInput_->disconnectTemporarily(seconds);

    // We set this to let the app know the remote is ready to be connected to
    std::string dfuString = "DFU-";
    auto ptBuild = prefs_->lastPageTurnerBuild.get();
    if (ptBuild.has_value()) {
        std::string build = ptBuild.value();
        if (build.find("DFU-") == std::string::npos) {
            dfuString += build;
        } else {
            dfuString = build;
        }
    }
    prefs_->lastPageTurnerBuild.set(dfuString);
#endif
}

void BLESettings::blinkLedAfterConnect(BLECharacteristic *characteristic, const std::string &key) {
    // TODO: Fully take over the LED for a while
    gpio_hold_dis(static_cast<gpio_num_t>(CONFIG_RED_LED));
    gpio_hold_dis(static_cast<gpio_num_t>(CONFIG_GREEN_LED));
#if CONFIG_BLUE_LED != -1
    gpio_hold_dis(static_cast<gpio_num_t>(CONFIG_BLUE_LED));
#endif
    for (int i = 0; i < 10; i++) {
        // Flash all the LEDs
        DigitalWrite(CONFIG_RED_LED, LED_ON);
        vTaskDelay(200 / portTICK_PERIOD_MS);
        DigitalWrite(CONFIG_RED_LED, LED_OFF);
        DigitalWrite(CONFIG_GREEN_LED, LED_ON);
        vTaskDelay(200 / portTICK_PERIOD_MS);
        DigitalWrite(CONFIG_GREEN_LED, LED_OFF);
#if CONFIG_BLUE_LED != -1
        DigitalWrite(CONFIG_BLUE_LED, LED_ON);
        vTaskDelay(200 / portTICK_PERIOD_MS);
        DigitalWrite(CONFIG_BLUE_LED, LED_OFF);
#endif
    }

    std::string value = StringFormat("%s=1", key.c_str());
    characteristic->setValue(value);
    characteristic->notify();
}

void BLESettings::unpairPageTurner(BLECharacteristic *characteristic, const std::string &key) {
    log_i("GATT client unpairing page turner");

#if CONFIG_USE_TURN_TOUCH_REMOTE
    ttRemoteInput_->unpairAndDisconnect();
#endif

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
