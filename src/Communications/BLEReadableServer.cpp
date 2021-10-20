
#include "BLEReadableServer.hpp"

#include <Arduino.h>

#include "sindarin-debug.h"

#define READABLE_CHARACTERISTIC_UUID "6851fc86-9284-4ff9-9ea8-82e22bc2f1b1"

void BLEReadableServer::setup() {
    BLECharacteristic *characteristic = BLEManager::getInstance()->service->createCharacteristic(
        BLEUUID(READABLE_CHARACTERISTIC_UUID), NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::WRITE);

    characteristic->setCallbacks(new CharacteristicCallbacks(this));
}
