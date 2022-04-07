#if USE_TURN_TOUCH_REMOTE

#include "TTRemoteInput.hpp"

#include <functional>

#include "sindarin-debug.h"

using namespace std::placeholders;

void TTRemoteInput::startRemote(int receivePinInput) { assert(0); }

void TTRemoteInput::startRemote() {}

auto TTRemoteInput::connectToDevice(BLEAdvertisedDevice *device) -> bool {
    if (client_ == nullptr) {
        log_d("Creating Turn Touch client");
        client_ = BLEDevice::createClient();
        client_->setClientCallbacks(new ClientCallbacks(this));
    }

    log_d("Attempting to connect to TurnTouch at address: %s",
          device->getAddress().toString().c_str());

    if (!client_->connect(device)) {
        log_e("Failed to connect to TurnTouch");
        return false;
    }

    // Obtain a reference to the service we are after in the remote BLE server.
    BLERemoteService *buttonService = client_->getService(TURNTOUCH_BUTTON_SVC_UUID);
    if (buttonService == nullptr) {
        log_e("Failed to find TurnTouch button service UUID.");
        client_->disconnect();
        return false;
    }
    log_d("TurnTouch service found.");

    // Obtain a reference to the characteristic in the service of the remote BLE server.
    BLERemoteCharacteristic *buttonCharacteristic =
        buttonService->getCharacteristic(TURNTOUCH_BUTTON_CHR_UUID);
    if (buttonCharacteristic == nullptr) {
        log_e("Failed to find TurnTouch button characteristic UUID.");
        client_->disconnect();
        return false;
    }
    log_d("BLE characteristic found");

    // Read the value of the characteristic.
    if (buttonCharacteristic->canRead()) {
        auto value = buttonCharacteristic->readValue<uint16_t>();
        log_d("The characteristic value was: %X", value);
    }

    if (buttonCharacteristic->canNotify()) {
        auto callback = [this](auto &&pH1, auto &&pH2, auto &&pH3, auto &&pH4) {
            buttonNotificationCallback(
                std::forward<decltype(pH1)>(pH1), std::forward<decltype(pH2)>(pH2),
                std::forward<decltype(pH3)>(pH3), std::forward<decltype(pH4)>(pH4));
        };
        buttonCharacteristic->subscribe(true, callback);
    }

    BLERemoteService *batteryService = client_->getService(TURNTOUCH_BATTERY_SVC_ID);
    if (buttonService == nullptr) {
        log_e("Failed to find TurnTouch battery service ID.");
    }

    BLERemoteCharacteristic *batteryCharacteristic =
        batteryService->getCharacteristic(TURNTOUCH_BATTERY_CHR_ID);
    if (batteryCharacteristic == nullptr) {
        log_e("Failed to find TurnTouch battery characteristic UUID.");
    }

    uint8_t battery = 0;
    if (batteryCharacteristic->canRead()) {
        battery = batteryCharacteristic->readValue<uint8_t>();
    }

    // Save the id of this turn touch for future connection attempts, but only
    // if it wasn't set already. Strange behavior where connecting to it after
    // a disconnect sometimes garbles the device->getAddress. For further exploration.
    if (!this->prefs_.pairedTurnTouchAddress.get().has_value()) {
        this->prefs_.pairedTurnTouchAddress.set(device->getAddress());
    }

    log_i("Connected to TurnTouch %s (battery: %d%%)", device->getAddress().toString().c_str(),
          battery);

    return true;
}

void TTRemoteInput::doneScanning(const NimBLEScanResults &results) {
    log_d("TurnTouch scan completed.");
    this->lastScanMs_ = millis();
}

void TTRemoteInput::scanForDevice() {
    digitalWrite(RED_LED, HIGH);

    // Run a scan for 1 second
    auto address = this->prefs_.pairedTurnTouchAddress.get();
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
    scan->start(10, nullptr, false);
    lastScanMs_ = millis();
    digitalWrite(RED_LED, LOW);
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
        bleCode_ = PREV;
        break;
    case TT_S_PRESS:
    case TT_S_DOUBLE:
        bleCode_ = NEXT;
        break;
    case TT_S_HOLD:
        bleCode_ = STOP;
        break;
    case TT_W_PRESS:
        bleCode_ = CHANNEL_MINUS;
        break;
    case TT_W_HOLD:
        bleCode_ = BACK;
        break;
    case TT_E_PRESS:
        bleCode_ = CHANNEL_PLUS;
        break;
    case TT_MULTI_N_S:
        bleCode_ = SETUP;
        break;
    case TT_MULTI_E_W:
        bleCode_ = ENTER;
        break;
    case TT_MULTI_E_N_W:
        bleCode_ = FACTORY_DEFAULTS;
        break;
    // Be careful with the double press codes, since you also get two single press commands
    // immediately before
    case TT_W_DOUBLE:
    case TT_E_DOUBLE:
    case TT_E_HOLD:
        bleCode_ = NONE;
        break;
    default:
        log_w("Invalid code processed from Turntouch: %X", processed);
        break;
    }
    wasCodeProcessed_ = 0;
}

auto TTRemoteInput::checkPress() -> int {
    // This isn't the best place for all of this, but since we have a contract with clients
    // that they have to check_presses, this is a reasonable spot to do this work.
    if (!deviceConnected_ && turntouchDevice_ != nullptr) {
        // TurnTouch has been found during scanning, attempt connection
        deviceConnected_ = connectToDevice(turntouchDevice_);
    } else if (!deviceConnected_ && !BLEDevice::getScan()->isScanning() &&
               millis() > (lastScanMs_ + RESCAN_INTERVAL_MS)) {
        scanForDevice();
    }
    return 0;
}

auto TTRemoteInput::checkRelease() -> int {
    int foundRelease = 0;
    if (!wasCodeProcessed_) {
        wasCodeProcessed_ = 1;
        lastCode_ = (RemoteCommand)bleCode_;
        foundRelease = 1;
        log_d("TurnTouch code: %s", CodeToString(lastCode_));
    }
    return foundRelease;
}

#endif
