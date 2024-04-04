#pragma once

#include <NimBLEDevice.h>
#include <memory>

#include "Communications/BLEManager.hpp"
#include "Misc/Telemetry.hpp"
#include "RemoteInput.hpp"
#include "Storage/PreferencesStore.hpp"
#include "TurnTouch.hpp"
#include "sindarin-debug.h"

#define RESCAN_INTERVAL_MS 3000
#define RESCAN_GAP_MS 100

class TTRemoteInput : public RemoteInput {
public:
    TTRemoteInput(PreferencesStore &prefs, Telemetry &telemetry) noexcept
        : prefs_(prefs), telemetry_(telemetry), pairedDeviceScanCallbacks_(this),
          newDeviceScanCallbacks_(this), clientCallbacks_(this){};
    TTRemoteInput(TTRemoteInput const &) = delete;

    // Scan for a PageTurner. Can be safely called even if one is already connected.
    void scanIfNecessary();
    void unpairAndDisconnect();
    void disconnectTemporarily(int seconds);
    auto connected() -> bool { return state_ == CONNECTED; }

protected:
    enum State {
        SCANNING,
        CONNECTING,
        CONNECTED,
    };

    void buttonNotificationCallback(BLERemoteCharacteristic *remoteCharacteristic, uint8_t *data,
                                    size_t length, bool isNotify);

    PreferencesStore &prefs_;
    Telemetry &telemetry_;
    State state_ = SCANNING;
    long lastScanMs_ = 0;
    long keepDisconnectedUntilMs_ = 0;
    BLEClient *client_ = nullptr;

    auto connectToDevice(BLEAdvertisedDevice &device) -> bool;
    void dispatchConnection(BLEAdvertisedDevice &device);
    void scanForNewDevice();
    void scanForPairedDevice();
    void scanForDevice();

    class PairedDeviceScanCallbacks : public NimBLEScanCallbacks {
    public:
        TTRemoteInput *enclosed;

        PairedDeviceScanCallbacks(TTRemoteInput *enclosure) : enclosed(enclosure) {}

        void onResult(BLEAdvertisedDevice *advertisedDevice) override {
            if (advertisedDevice->haveServiceUUID()) {

                // log_v("BLEAdvertisedDevice w/ services found: %s",
                //       advertisedDevice->getName().c_str());
                // for (int i = 0; i < advertisedDevice->getServiceUUIDCount(); ++i) {
                //     log_v("Service %u - %s", i,
                //           advertisedDevice->getServiceUUID(i).toString().c_str());
                // }
                // Connect to device if this is the paired device
                std::optional<BLEAddress> address = enclosed->prefs_.pairedTurnTouchAddress.get();
                if (advertisedDevice->isAdvertisingService(BLEUUID(TURNTOUCH_BUTTON_SVC_ID)) &&
                    (address.has_value() && address.value() == advertisedDevice->getAddress())) {
                    SHOW_HEAP();
                    log_i("%s PageTurner found; queueing for connect...",
                          address.has_value() ? "Paired" : "New");
                    BLEDevice::getScan()->stop();
                    enclosed->dispatchConnection(*advertisedDevice);
                }
            }
        }
    };

    PairedDeviceScanCallbacks pairedDeviceScanCallbacks_;

    class NewDeviceScanCallbacks : public NimBLEScanCallbacks {
    public:
        TTRemoteInput *enclosed;

        NewDeviceScanCallbacks(TTRemoteInput *enclosure) : enclosed(enclosure) {}

        void onScanEnd(NimBLEScanResults results) override {
            if (results.getCount() == 0) {
                log_w("Absolutely 0 BLE devices found.");
                return;
            }

            std::vector<NimBLEAdvertisedDevice *> filteredDevices;

            for (auto advertisedDevice : results) {
                if (advertisedDevice->isAdvertisingService(BLEUUID(TURNTOUCH_BUTTON_SVC_ID))) {
                    filteredDevices.push_back(advertisedDevice);
                }
            }

            // Sort devices by RSSI (descending order)
            std::sort(filteredDevices.begin(), filteredDevices.end(),
                      [](NimBLEAdvertisedDevice *a, NimBLEAdvertisedDevice *b) {
                return a->getRSSI() > b->getRSSI();
            });

            log_i("Scan complete. Found %zu PageTurner devices (%d total BLE devices)",
                  filteredDevices.size(), results.getCount());

            if (filteredDevices.empty()) {
                return;
            }
            for (auto advertisedDevice : filteredDevices) {
                log_d("PageTurner: %s, RSSI: %d", advertisedDevice->getAddress().toString().c_str(),
                      advertisedDevice->getRSSI());
            }

            enclosed->dispatchConnection(*(filteredDevices[0]));
        }
    };

    NewDeviceScanCallbacks newDeviceScanCallbacks_;

    class ClientCallbacks : public BLEClientCallbacks {
    public:
        TTRemoteInput *enclosed;

        ClientCallbacks(TTRemoteInput *enclosure) : enclosed(enclosure) {}

        void onConnect(BLEClient *client) override {
            log_d("BLE PageTurner connected!");
            enclosed->state_ = CONNECTED;
        }

        void onDisconnect(NimBLEClient *client, int reason) override {
            log_w("BLE PageTurner disconnected!");
            enclosed->state_ = SCANNING;
            if (enclosed->keepDisconnectedUntilMs_ != 0 &&
                enclosed->keepDisconnectedUntilMs_ > Millis()) {
                log_i("BLE PageTurner disconnected!");
            } else {
                log_w("Unexpected BLE PageTurner disconnect!");
                enclosed->telemetry_.logMetric(TelemetryField::METRIC_BLE, "remote_disconnect");
            }
        }
    };

    ClientCallbacks clientCallbacks_;
};
