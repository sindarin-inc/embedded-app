#include "TTRemoteInput.hpp"
#include "sindarin-debug.h"
#include <Arduino.h>
#include <functional>

using namespace std::placeholders;

void TTRemoteInput::start_remote(int receive_pin_input)
{
    DEBUG_LN("FAIL DUE TO BLE CONSTUCTOR NOT NEEDING THIS ONE");
    assert(0);
}

void TTRemoteInput::start_remote()
{
    last_code = -1;
    ble_code = -1;
    // No code to process, so we set this to true
    was_code_processed = 1;

    deviceConnected = false;
    wasDeviceConnected = false;

    // Ensure BLEManager is initialized by this point
    BLEManager::getInstance();
    scanForDevice();
}

void TTRemoteInput::connectToDevice(BLEAdvertisedDevice *device)
{
    DEBUG_LN("Attempting to connect to BLE address:");
    DEBUG_LN(device->getAddress().toString().c_str());

    BLEClient *client = BLEManager::getInstance()->client;
    DEBUG_LN("Setting client callbacks");
    client->setClientCallbacks(new ClientCallbacks(this));
    DEBUG_LN("Connecting...");
    client->connect(device);
    DEBUG_LN("Connected to BLE server for TurnTouch");

    // Obtain a reference to the service we are after in the remote BLE server.
    BLERemoteService *remoteService = client->getService(TURNTOUCH_BUTTON_SVC_UUID);
    if (remoteService == nullptr)
    {
        DEBUG_LN("Failed to find TurnTouch button service UUID.");
        client->disconnect();
        return;
    }
    DEBUG_LN("BLE service found.");

    // Obtain a reference to the characteristic in the service of the remote BLE server.
    BLERemoteCharacteristic *remoteCharacteristic = remoteService->getCharacteristic(TURNTOUCH_BUTTON_CHR_UUID);
    if (remoteCharacteristic == nullptr)
    {
        DEBUG_LN("Failed to find TurnTouch button characteristic UUID.");
        client->disconnect();
        return;
    }
    DEBUG_LN("BLE characteristic found");

    // Read the value of the characteristic.
    if (remoteCharacteristic->canRead())
    {
        uint16_t value = remoteCharacteristic->readUInt16();
        DEBUG_LN("The characteristic value was: ");
        DEBUG_LN(value);
    }

    if (remoteCharacteristic->canNotify())
    {
        auto callback = std::bind(&TTRemoteInput::buttonNotificationCallback, this, _1, _2, _3, _4);
        remoteCharacteristic->registerForNotify(callback);
    }

    deviceConnected = true;
}

void TTRemoteInput::scanForDevice()
{
    // Run a scan for 3 seconds
    DEBUG_LN("Starting BLE scan...");
    BLEScan *scan = BLEDevice::getScan();
    scan->setAdvertisedDeviceCallbacks(new AdvertisedDeviceCallbacks(this));
    scan->setInterval(1349);
    scan->setWindow(449);
    scan->setActiveScan(true);
    scan->start(3, false);
    DEBUG_LN("BLE scan completed.");
    lastScanMs = millis();
}

void TTRemoteInput::buttonNotificationCallback(BLERemoteCharacteristic *remoteCharacteristic, uint8_t *data, size_t length, bool isNotify)
{
    if (length != 2)
    {
        DEBUG_F("WARNING: Expected 2 byte notification from Turntouch; got %u; bailing early\n", length);
        return;
    }
    DEBUG_F("0 - %02x; 1 - %02x\n", data[0], data[1]);
    uint16_t processed = (data[0] << 8) | (data[1]);
    DEBUG_F("processed: %u\n", processed);
    DEBUG_F("Notify callback for characteristic %s of data length %u content: %u\n", remoteCharacteristic->getUUID().toString().c_str(), length, processed);
    if (processed == TT_NONE)
    {
        DEBUG_LN("Turntouch sent code NONE, bailing early");
        return;
    }
    switch (processed)
    {
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
    case TT_W_HOLD:
        ble_code = CHANNEL_MINUS;
        break;
    case TT_E_PRESS:
    case TT_E_DOUBLE:
    case TT_E_HOLD:
        ble_code = CHANNEL_PLUS;
        break;
    case TT_MULTI_N_S:
        ble_code = SETUP;
        break;
    default:
        DEBUG_F("Invalid code processed from Turntouch: %u", processed);
        break;
    }
    was_code_processed = 0;
}

int TTRemoteInput::check_press()
{
    // This isn't the best place for all of this, but since we have a contract with clients
    // that they have to check_presses, this is a reasonable spot to do this work.

    if (!deviceConnected && turntouchDevice != nullptr)
    {
        DEBUG_F("Attempting connection to %s!\n", turntouchDevice->toString().c_str());
        connectToDevice(turntouchDevice);
    }
    if (!deviceConnected && millis() > (lastScanMs + RESCAN_INTERVAL_MS))
    {
        DEBUG_LN("No Turntouch connected; restarting bluetooth scanning");
        scanForDevice();
        wasDeviceConnected = deviceConnected;
    }
    if (deviceConnected && !wasDeviceConnected)
    {
        // do any additional desired stuff here on connecting
        wasDeviceConnected = deviceConnected;
    }
    return 0;
}

int TTRemoteInput::check_release()
{
    int found_release = 0;
    if (!was_code_processed)
    {
        was_code_processed = 1;
        last_code = ble_code;
        found_release = 1;
        print_out_code("Got a BLE code", last_code);
    }
    return found_release;
}
