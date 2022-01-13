#include "TTRemoteInput.hpp"

#include <Arduino.h>
#include <functional>

#include "sindarin-debug.h"

using namespace std::placeholders;

void TTRemoteInput::start_remote(int receive_pin_input) {
    DEBUG_LN("FAIL DUE TO BLE CONSTUCTOR NOT NEEDING THIS ONE");
    assert(0);
}

void TTRemoteInput::start_remote() {}

bool TTRemoteInput::connectToDevice(BLEAdvertisedDevice *device) {
    if (client == nullptr) {
        log_d("Creating Turn Touch client");
        client = BLEDevice::createClient();
        client->setClientCallbacks(new ClientCallbacks(this));
    }

    log_d("Attempting to connect to TurnTouch at address: %s",
          device->getAddress().toString().c_str());

    if (!client->connect(device)) {
        log_e("Failed to connect to TurnTouch");
        return false;
    }

    // Obtain a reference to the service we are after in the remote BLE server.
    BLERemoteService *remoteService = client->getService(TURNTOUCH_BUTTON_SVC_UUID);
    if (remoteService == nullptr) {
        log_e("Failed to find TurnTouch button service UUID.");
        client->disconnect();
        return false;
    }
    log_d("TurnTouch service found.");

    // Obtain a reference to the characteristic in the service of the remote BLE server.
    BLERemoteCharacteristic *remoteCharacteristic =
        remoteService->getCharacteristic(TURNTOUCH_BUTTON_CHR_UUID);
    if (remoteCharacteristic == nullptr) {
        log_e("Failed to find TurnTouch button characteristic UUID.");
        client->disconnect();
        return false;
    }
    log_d("BLE characteristic found");

    // Read the value of the characteristic.
    if (remoteCharacteristic->canRead()) {
        uint16_t value = remoteCharacteristic->readValue<uint16_t>();
        log_d("The characteristic value was: %X", value);
    }

    if (remoteCharacteristic->canNotify()) {
        auto callback = std::bind(&TTRemoteInput::buttonNotificationCallback, this, _1, _2, _3, _4);
        remoteCharacteristic->subscribe(true, callback);
    }

    // Save the id of this turn touch for future connection attempts
    this->prefs.pairedTurnTouchAddress.set(device->getAddress());

    log_i("Connected to TurnTouch");
    return true;
}

void TTRemoteInput::doneScanning(NimBLEScanResults results) {
    log_d("TurnTouch scan completed.");
    this->lastScanMs = millis();
}

void TTRemoteInput::scanForDevice() {
    // Run a scan for 3 seconds
    auto address = this->prefs.pairedTurnTouchAddress.get();
    if (address.has_value()) {
        log_d("Scanning for paired TurnTouch %s", std::string(address.value()).c_str());
    } else {
        log_d("Scanning for any available TurnTouch...");
    }
    BLEScan *scan = BLEDevice::getScan();
    scan->setAdvertisedDeviceCallbacks(new AdvertisedDeviceCallbacks(this));
    scan->setInterval(1349);
    scan->setWindow(449);
    scan->setActiveScan(true);
    // TODO: It'd be nice to use the non-blocking call here, but that doesn't take a std::function
    // so passing in a callback with access to `this` is a little clunky
    scan->start(3, false);
    log_d("BLE scan completed.");
    lastScanMs = millis();
}

void TTRemoteInput::buttonNotificationCallback(BLERemoteCharacteristic *remoteCharacteristic,
                                               uint8_t *data, size_t length, bool isNotify) {
    if (length != 2) {
        log_w("WARNING: Expected 2 byte notification from Turntouch; got %u; bailing early",
              length);
        return;
    }
    log_v("0 - %02x; 1 - %02x", data[0], data[1]);
    uint16_t processed = (data[0] << 8) | (data[1]);
    log_v("processed: %u", processed);
    log_v("Notify callback for characteristic %s of data length %u content: %u",
          remoteCharacteristic->getUUID().toString().c_str(), length, processed);
    if (processed == TT_NONE) {
        log_v("Turntouch sent code NONE, bailing early");
        return;
    }
    switch (processed) {
    case TT_N_PRESS:
    case TT_N_DOUBLE:
    case TT_N_HOLD:
        ble_code = PREV;
        break;
    case TT_S_PRESS:
    case TT_S_DOUBLE:
        ble_code = NEXT;
        break;
    case TT_S_HOLD:
        ble_code = STOP;
        break;
    case TT_W_PRESS:
    case TT_W_DOUBLE:
        ble_code = CHANNEL_MINUS;
        break;
    case TT_W_HOLD:
        ble_code = BACK;
        break;
    case TT_E_PRESS:
    case TT_E_DOUBLE:
    case TT_E_HOLD:
        ble_code = CHANNEL_PLUS;
        break;
    case TT_MULTI_N_S:
        ble_code = SETUP;
        break;
    case TT_MULTI_E_W:
        ble_code = ENTER;
        break;
    case TT_MULTI_E_N_W:
        ble_code = FACTORY_DEFAULTS;
        break;
    default:
        log_w("Invalid code processed from Turntouch: %X", processed);
        break;
    }
    was_code_processed = 0;
}

auto TTRemoteInput::check_press() -> int {
    // This isn't the best place for all of this, but since we have a contract with clients
    // that they have to check_presses, this is a reasonable spot to do this work.
    if (!deviceConnected && turntouchDevice != nullptr) {
        // TurnTouch has been found during scanning, attempt connection
        deviceConnected = connectToDevice(turntouchDevice);
    } else if (!deviceConnected && millis() > (lastScanMs + RESCAN_INTERVAL_MS)) {
        scanForDevice();
    }
    return 0;
}

auto TTRemoteInput::check_release() -> int {
    int found_release = 0;
    if (!was_code_processed) {
        was_code_processed = 1;
        last_code = (RemoteCommand)ble_code;
        found_release = 1;
        log_d("Got a TurnTouch code: %s", codeToString(last_code));
    }
    return found_release;
}
