
#include "BLERemoteInput.hpp"

#include <Arduino.h>

#include "sindarin-debug.h"

#define CHARACTERISTIC_UUID "ed0c3501-a3cb-499f-93ad-2acbaa971d63"

void BLERemoteInput::start_remote(int receive_pin_input) {
    DEBUG_LN("FAIL DUE TO BLE CONSTUCTOR NOT NEEDING THIS ONE");
    assert(0);
}

void BLERemoteInput::start_remote() {
    last_code = RemoteCommand::NONE;
    ble_code = RemoteCommand::NONE;
    // No code to process, so we set this to true
    was_code_processed = 1;

    deviceConnected = false;
    wasDeviceConnected = false;

    SHOW_HEAP();
    BLEManager::getInstance()->server->setCallbacks(new ServerCallbacks(this));

    BLECharacteristic *characteristic = BLEManager::getInstance()->service->createCharacteristic(
        BLEUUID(CHARACTERISTIC_UUID), NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::WRITE);

    characteristic->setCallbacks(new CharacteristicCallbacks(this));

    characteristic->setValue("Hello World");
}

auto BLERemoteInput::check_press() -> int {
    // This isn't the best place for this, but since we have a contract with clients
    // that they have to check_presses, this is a reasonable spot to do this work.

    // TODO: Refactor this out into a proper abstraction (probably at least two actually) -cojo
    if (!deviceConnected && wasDeviceConnected) {
        delay(500); // give the bluetooth stack the chance to get things ready for restart
        BLEManager::getInstance()->server->startAdvertising(); // restart advertising
        DEBUG_LN("Restarting bluetooth advertising");
        wasDeviceConnected = deviceConnected;
    }
    if (deviceConnected && !wasDeviceConnected) {
        // do any additional desired stuff here on connecting
        wasDeviceConnected = deviceConnected;
    }
    // END TODO "Refactor this out... -cojo"
    return 0;
}

auto BLERemoteInput::check_release() -> int {
    int found_release = 0;
    if (!was_code_processed) {
        was_code_processed = 1;
        last_code = (RemoteCommand)ble_code;
        found_release = 1;
        print_out_code("Got a BLE code", last_code);
    }
    return found_release;
}
