#if CONFIG_USE_TURN_TOUCH_REMOTE

#include "TTRemoteInput.hpp"

#include <algorithm>
#include <functional>

#include "Misc/Dispatch.hpp"
#include "Misc/Platform.hpp"
#include "Misc/SemVer.hpp"
#include "TurnTouch.hpp"
#include "sindarin-debug.h"

// These should match the page turner `CONFIG_BT_PERIPHERAL_PREF_` settings here:
// https://github.com/sindarin-inc/page-turner-nrf52/blob/master/prj.conf#L28
#define CONNECTION_INTERVAL_MIN 40
#define CONNECTION_INTERVAL_MAX 90
#define CONNECTION_LATENCY 20
#define CONNECTION_TIMEOUT 1000

using namespace std::placeholders;

auto TTRemoteInput::connectToDevice(BLEAdvertisedDevice &device) -> bool {
    if (client_ == nullptr) {
        client_ = BLEDevice::createClient();
        client_->setClientCallbacks(&clientCallbacks_);
    }

    // Update connection parameters after establishing a connection
    client_->setConnectionParams(CONNECTION_INTERVAL_MIN, CONNECTION_INTERVAL_MAX,
                                 CONNECTION_LATENCY, CONNECTION_TIMEOUT);

    BLEAddress address = device.getAddress();
    log_d("Attempting to connect to PageTurner at address: %s", address.toString().c_str());

    if (!client_->connect(&device)) {
        log_e("Failed to connect to PageTurner");
        return false;
    }
    // Obtain a reference to the service we are after in the remote BLE server.
    BLERemoteService *buttonService = client_->getService(TURNTOUCH_BUTTON_SVC_UUID);
    if (buttonService == nullptr) {
        log_e("Failed to find PageTurner button service UUID.");
        return false;
    }
    log_d("PageTurner service found.");

    // Obtain a reference to the characteristic in the service of the remote BLE server.
    BLERemoteCharacteristic *buttonCharacteristic =
        buttonService->getCharacteristic(TURNTOUCH_BUTTON_CHR_UUID);
    if (buttonCharacteristic == nullptr) {
        log_e("Failed to find PageTurner button characteristic UUID.");
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
            log_e("Failed to find PageTurner battery service ID.");
            return;
        }

        BLERemoteCharacteristic *batteryCharacteristic =
            batteryService->getCharacteristic(TURNTOUCH_BATTERY_CHR_ID);
        if (batteryCharacteristic == nullptr) {
            log_e("Failed to find PageTurner battery characteristic UUID.");
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
            log_w("Failed to find PageTurner Device Information Service ID.");
            // return false; // Don't exit if not found, simply means an older remote
        } else {
            BLERemoteCharacteristic *buildCharacteristic =
                disService->getCharacteristic(TURNTOUCH_BUILD_CHR_ID);
            if (buildCharacteristic == nullptr) {
                log_e("Failed to find PageTurner build characteristic UUID.");
                return;
            }

            if (buildCharacteristic->canRead()) {
                build = buildCharacteristic->readValue();
                prefs_.lastPageTurnerBuild.set(build);
                log_d("PageTurner build: %s", build.c_str());
            }
        }

        // Save the id of this turn touch for future connection attempts, but only
        // if it wasn't set already.
        if (!prefs_.pairedTurnTouchAddress.get().has_value()) {
            prefs_.pairedTurnTouchAddress.set(address);
        }
    });

    SHOW_HEAP();

    log_i("Connected to PageTurner %s", address.toString().c_str());

    // Output a metric log on the very first TT connection
    static bool sFirstConnection = true;
    if (sFirstConnection) {
        log_i("First TT connection in %.0f ms", esp_timer_get_time() / 1000.0f);
        sFirstConnection = false;
    }

    return true;
}

void TTRemoteInput::dispatchConnection(BLEAdvertisedDevice &device) {
    state_ = CONNECTING;
    auto dispatched = DispatchQueue::dispatchOneOff(
        [this, &device] {
        if (!connectToDevice(device)) {
            client_->disconnect();
            state_ = SCANNING;
        }
        },
        4000, 3, 0);
    if (!dispatched) {
        log_e("dispatchConnection failed");
        client_->disconnect();
        state_ = SCANNING;
    }
}

void TTRemoteInput::unpairAndDisconnect() {
    prefs_.lastPageTurnerBuild.remove();
    prefs_.pairedTurnTouchAddress.remove();
    // Setting a nonzero keepDisconnectedUntilMs_ helps with two things:
    // 1) It is the signal that the disconnect was intentional
    // 2) Gives the remote a moment to go back to advertising
    disconnectTemporarily(1);
}

void TTRemoteInput::disconnectTemporarily(int seconds) {
    if (client_ != nullptr) {
        client_->disconnect();
    }
    state_ = SCANNING;
    keepDisconnectedUntilMs_ = Millis() + seconds * 1000;
}

void TTRemoteInput::scanForNewDevice() {
    auto scan = BLEDevice::getScan();
    scan->setInterval(40);
    scan->setWindow(30);
    scan->setActiveScan(true);
    scan->setScanCallbacks(&newDeviceScanCallbacks_);
    auto started = scan->start(10000, false);
    if (!started) {
        log_e("Failed to start scan");
        return;
    }
}

void TTRemoteInput::scanForPairedDevice() {
    // PT build 0.2.0 and earlier only support active scanning, which is less power efficient
    bool useActiveScan = false;
    auto ptBuild = prefs_.lastPageTurnerBuild.get();
    if (ptBuild.has_value()) {
        std::string build = ptBuild.value();
        // If `build` has a prefix of "DFU-", strip it off
        if (build.find("DFU-") == 0) {
            build = build.substr(4);
        }
        if (SemVerCompareVersions(build, "0.2.99") == SemVerComparison::LessThan) {
            log_i("Using active scan for PT build %s", build.c_str());
            useActiveScan = true;
        }
    } else {
        // If we're unsure, just revert to using active scan
        log_i("Using active scan for PT because we have yet to connect to one");
        useActiveScan = true;
    }

    BLEScan *scan = BLEDevice::getScan();
    scan->setScanCallbacks(&pairedDeviceScanCallbacks_);
    // See: https://electronics.stackexchange.com/a/359158/16459
    // Only one advertising channel is being looked at during each
    // scanInterval and is rotated to the next of 3 channels with each interval. So if there is
    // RF interference on that channel you might not see the device even if you have the
    // scanWindow equal to the scanInterval in order to do continuous scanning.
    scan->setInterval(40);
    scan->setWindow(30);
    scan->setActiveScan(useActiveScan);
    auto started = scan->start(RESCAN_INTERVAL_MS, false);
    if (!started) {
        log_e("Failed to start scan");
        return;
    }
}

void TTRemoteInput::scanForDevice() {
    DigitalWrite(CONFIG_BLUE_LED, LED_ON);
    auto address = this->prefs_.pairedTurnTouchAddress.get();
    if (address.has_value()) {
        log_i("Scanning for paired PageTurner %s", std::string(address.value()).c_str());
        scanForPairedDevice();
    } else {
        log_i("Scanning for any available PageTurner...");
        scanForNewDevice();
    }
    lastScanMs_ = Millis();
    DigitalWrite(CONFIG_BLUE_LED, LED_OFF);
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
    // Used by the final tester to check button presses
    log_i("PT: %s", CodeToString(command));
    if (command != RemoteCommand::NONE) {
        this->callback_(command);
    } else {
        log_w("Invalid code processed from Turntouch: %X", processed);
    }
}

void TTRemoteInput::scanIfNecessary() {
    if (state_ == SCANNING && !BLEDevice::getScan()->isScanning() &&
        (keepDisconnectedUntilMs_ == 0 || Millis() >= keepDisconnectedUntilMs_) &&
        (lastScanMs_ == 0 || Millis() > (lastScanMs_ + RESCAN_INTERVAL_MS + RESCAN_GAP_MS))) {
        lastScanMs_ = Millis();
        scanForDevice();
    }

    if (Millis() < keepDisconnectedUntilMs_) {
        log_d("Waiting to reconnect to PageTurner until %lu", keepDisconnectedUntilMs_);
    }
}

#endif
