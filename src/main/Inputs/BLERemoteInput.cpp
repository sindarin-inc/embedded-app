#if USE_BLE_SERVER_REMOTE

#include "BLERemoteInput.hpp"

#include "sindarin-debug.h"

#define CHARACTERISTIC_UUID "ed0c3501-a3cb-499f-93ad-2acbaa971d63"

void BLERemoteInput::configureCharacteristics(BLEService *service) {
    BLECharacteristic *characteristic = service->createCharacteristic(
        BLEUUID(CHARACTERISTIC_UUID), NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::WRITE);
    characteristic->setCallbacks(new CharacteristicCallbacks(this));
    characteristic->setValue("Hello World");
}

#endif
