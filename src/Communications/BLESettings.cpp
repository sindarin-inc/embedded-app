
#include "BLESettings.hpp"
#include "sindarin-debug.h"
#include <Arduino.h>

#define SETTINGS_CHARACTERISTIC_UUID "611f68af-5ffe-4944-9b8f-ef6773326268"

BLESettings::BLESettings(Preferences *prefs) {
  this->prefs = prefs;
}

void BLESettings::listen_for_settings() {
  BLECharacteristic *characteristic = BLEManager::getInstance()->service->createCharacteristic(
      BLEUUID(SETTINGS_CHARACTERISTIC_UUID),
      NIMBLE_PROPERTY::WRITE);

  characteristic->setCallbacks(new CharacteristicCallbacks(this));
}
