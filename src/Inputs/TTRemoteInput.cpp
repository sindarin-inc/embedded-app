#if USE_TURN_TOUCH_REMOTE

#include "TTRemoteInput.hpp"

#include <functional>

#include "Misc/Dispatch.hpp"
#include "Misc/Platform.hpp"
#include "TurnTouch.hpp"
#include "sindarin-debug.h"

using namespace std::placeholders;

auto TTRemoteInput::connectToDevice(const std::shared_ptr<BLEAdvertisedDevice> &device) -> bool {
    if (client_ == nullptr) {
        log_d("Creating Turn Touch client");
        client_ = BLEDevice::createClient();
        client_->setClientCallbacks(&clientCallbacks_);
    }

    BLEAddress address = device->getAddress();
    log_d("Attempting to connect to TurnTouch at address: %s", address.toString().c_str());

    if (!client_->connect(device.get())) {
        log_e("Failed to connect to TurnTouch");
        return false;
    }

    // Obtain a reference to the service we are after in the remote BLE server.
    BLERemoteService *buttonService = client_->getService(TURNTOUCH_BUTTON_SVC_UUID);
    if (buttonService == nullptr) {
        log_e("Failed to find TurnTouch button service UUID.");
        return false;
    }
    log_d("TurnTouch service found.");

    // Obtain a reference to the characteristic in the service of the remote BLE server.
    BLERemoteCharacteristic *buttonCharacteristic =
        buttonService->getCharacteristic(TURNTOUCH_BUTTON_CHR_UUID);
    if (buttonCharacteristic == nullptr) {
        log_e("Failed to find TurnTouch button characteristic UUID.");
        return false;
    }

    if (buttonCharacteristic->canNotify()) {
        auto callback = [this](auto &&pH1, auto &&pH2, auto &&pH3, auto &&pH4) {
            buttonNotificationCallback(
                std::forward<decltype(pH1)>(pH1), std::forward<decltype(pH2)>(pH2),
                std::forward<decltype(pH3)>(pH3), std::forward<decltype(pH4)>(pH4));
        };
        buttonCharacteristic->subscribe(true, callback);
    }

    // Discover non-immediate services and characteristics asynchronously
    auto queue = DispatchGetCore0BackgroundQueue();
    queue->dispatch([this, address] {
        // Read the battery level from the battery service
        BLERemoteService *batteryService = client_->getService(TURNTOUCH_BATTERY_SVC_ID);
        if (batteryService == nullptr) {
            log_e("Failed to find TurnTouch battery service ID.");
            return;
        }

        BLERemoteCharacteristic *batteryCharacteristic =
            batteryService->getCharacteristic(TURNTOUCH_BATTERY_CHR_ID);
        if (batteryCharacteristic == nullptr) {
            log_e("Failed to find TurnTouch battery characteristic UUID.");
            return;
        }

        uint8_t battery = 0;
        if (batteryCharacteristic->canRead()) {
            battery = batteryCharacteristic->readValue<uint8_t>();
            PreferencesStore::lastPageTurnerBattery = battery;
            log_i("Page turner battery level: %d%%", battery);
        }

        // Read the build version from the Device Information Service (DIS)
        BLERemoteService *disService = client_->getService(TURNTOUCH_DIS_SVC_ID);
        std::string build;
        if (disService == nullptr) {
            log_w("Failed to find TurnTouch Device Information Service ID.");
            // return false; // Don't exit if not found, simply means an older remote
        } else {
            BLERemoteCharacteristic *buildCharacteristic =
                disService->getCharacteristic(TURNTOUCH_BUILD_CHR_ID);
            if (buildCharacteristic == nullptr) {
                log_e("Failed to find TurnTouch build characteristic UUID.");
                return;
            }

            if (buildCharacteristic->canRead()) {
                build = buildCharacteristic->readValue();
                prefs_.lastPageTurnerBuild.set(build);
                log_d("Page turner build: %s", build.c_str());
            }
        }

        // Save the id of this turn touch for future connection attempts, but only
        // if it wasn't set already.
        if (!prefs_.pairedTurnTouchAddress.get().has_value()) {
            prefs_.pairedTurnTouchAddress.set(address);
        }
    });

    log_i("Connected to TurnTouch %s", address.toString().c_str());

    // Output a metric log on the very first TT connection
    static bool sFirstConnection = true;
    if (sFirstConnection) {
        log_i("First TT connection in %.0f ms", esp_timer_get_time() / 1000.0f);
        sFirstConnection = false;
    }

    return true;
}

void TTRemoteInput::disconnectTemporarily(int seconds) {
    if (client_ != nullptr) {
        client_->disconnect();
    }
    state_ = SCANNING;
    keepDisconnectedUntilMs_ = Millis() + seconds * 1000;

    std::string dfuString = "DFU-";
    auto ptBuild = prefs_.lastPageTurnerBuild.get();
    if (ptBuild.has_value()) {
        std::string build = ptBuild.value();
        if (build.find("DFU-") == std::string::npos) {
            dfuString += build;
        } else {
            dfuString = build;
        }
    }
    prefs_.lastPageTurnerBuild.set(dfuString);
}

void TTRemoteInput::scanForDevice() {
    DigitalWrite(BLUE_LED, LED_ON);

    auto address = this->prefs_.pairedTurnTouchAddress.get();
    if (address.has_value()) {
        log_d("Scanning for paired TurnTouch %s", std::string(address.value()).c_str());
    } else {
        log_d("Scanning for any available TurnTouch...");
    }
    BLEScan *scan = BLEDevice::getScan();

    scan->setAdvertisedDeviceCallbacks(&advertisedDeviceCallbacks_);
    // See: https://electronics.stackexchange.com/a/359158/16459
    // Only one advertising channel is being looked at during each
    // scanInterval and is rotated to the next of 3 channels with each interval. So if there is
    // RF interference on that channel you might not see the device even if you have the
    // scanWindow equal to the scanInterval in order to do continuous scanning.
    scan->setInterval(40);
    scan->setWindow(30);
    scan->setActiveScan(true);
    scan->start((int)(RESCAN_INTERVAL_MS / 1000), nullptr, false);

    lastScanMs_ = Millis();
    DigitalWrite(BLUE_LED, LED_OFF);
}

void TTRemoteInput::buttonNotificationCallback(BLERemoteCharacteristic *remoteCharacteristic,
                                               uint8_t *data, size_t length, bool isNotify) {
    if (length != 2) {
        log_w("WARNING: Expected 2 byte notification from Turntouch; got %zu; bailing early",
              length);
        return;
    }
    log_v("0 - %02x; 1 - %02x", data[0], data[1]);
    uint16_t processed = (data[0] << 8) | (data[1]);
    log_v("processed: %u", processed);
    log_v("Notify callback for characteristic %s of data length %zu content: %u",
          remoteCharacteristic->getUUID().toString().c_str(), length, processed);
    if (processed == TT_NONE) {
        log_v("Turntouch sent code NONE, bailing early");
        return;
    }
    RemoteCommand command = DecodeTurntouch(processed);
    if (command != RemoteCommand::NONE) {
        this->callback_(command);
    } else {
        log_w("Invalid code processed from Turntouch: %X", processed);
    }
}

void TTRemoteInput::connect() {
    if (state_ == SCANNING && !BLEDevice::getScan()->isScanning() &&
        (keepDisconnectedUntilMs_ == 0 || Millis() >= keepDisconnectedUntilMs_) &&
        (lastScanMs_ == 0 || Millis() > (lastScanMs_ + RESCAN_INTERVAL_MS + RESCAN_GAP_MS))) {
        lastScanMs_ = Millis();
        scanForDevice();
    }

    if (Millis() < keepDisconnectedUntilMs_) {
        log_d("Waiting to reconnect to TurnTouch until %lu", keepDisconnectedUntilMs_);
    }
}

#endif
