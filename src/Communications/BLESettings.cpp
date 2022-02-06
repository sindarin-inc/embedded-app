#if USE_BLE_SERVER_REMOTE

#include "BLESettings.hpp"

#include <Arduino.h>

#include "sindarin-debug.h"

#define SETTINGS_CHARACTERISTIC_UUID "611f68af-5ffe-4944-9b8f-ef6773326268"

BLESettings::BLESettings(PreferencesStore *prefs) { this->prefs_ = prefs; }

void BLESettings::configureCharacteristics(BLEService *service) {
    BLECharacteristic *characteristic = service->createCharacteristic(
        BLEUUID(SETTINGS_CHARACTERISTIC_UUID), NIMBLE_PROPERTY::WRITE);

    characteristic->setCallbacks(new CharacteristicCallbacks(this));
};

#endif
