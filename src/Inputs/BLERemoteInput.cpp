#if USE_BLE_SERVER_REMOTE

#include "BLERemoteInput.hpp"

#include <Arduino.h>

#include "sindarin-debug.h"

#define CHARACTERISTIC_UUID "ed0c3501-a3cb-499f-93ad-2acbaa971d63"

void BLERemoteInput::configureCharacteristics(BLEService *service) {
    BLECharacteristic *characteristic = service->createCharacteristic(
        BLEUUID(CHARACTERISTIC_UUID), NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::WRITE);
    characteristic->setCallbacks(new CharacteristicCallbacks(this));
    characteristic->setValue("Hello World");
}

void BLERemoteInput::startRemote(int receivePinInput) {
    log_e("FAIL DUE TO BLE CONSTUCTOR NOT NEEDING THIS ONE");
    assert(0);
}

void BLERemoteInput::startRemote() {}

auto BLERemoteInput::checkPress() -> int { return 0; }

auto BLERemoteInput::checkRelease() -> int {
    int foundRelease = 0;
    if (!wasCodeProcessed_) {
        wasCodeProcessed_ = 1;
        lastCode_ = (RemoteCommand)bleCode_;
        foundRelease = 1;
        log_d("Got a BLE code: %s", CodeToString(lastCode_));
    }
    return foundRelease;
}

#endif
