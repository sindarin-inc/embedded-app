#ifndef TTRemoteInput_H
#define TTRemoteInput_H

#include <NimBLEDevice.h>
#include <memory>

#include "Communications/BLEManager.hpp"
#include "RemoteInput.hpp"
#include "Storage/PreferencesStore.hpp"
#include "TurnTouch.hpp"
#include "sindarin-debug.h"

#define RESCAN_INTERVAL_MS 3000
#define RESCAN_GAP_MS 100

class TTRemoteInput : public RemoteInput {
public:
    TTRemoteInput(PreferencesStore &prefs) noexcept
        : prefs_(prefs), advertisedDeviceCallbacks_(this), clientCallbacks_(this){};
    TTRemoteInput(TTRemoteInput const &) = delete;

    // Scan for a PageTurner. Can be safely called even if one is already connected.
    void scanIfNecessary();
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
    State state_ = SCANNING;
    long lastScanMs_ = 0;
    long keepDisconnectedUntilMs_ = 0;
    BLEClient *client_ = nullptr;

    auto connectToDevice(BLEAdvertisedDevice &device) -> bool;
    void dispatchConnection(BLEAdvertisedDevice &device);
    void scanForNewDevice();
    void scanForPairedDevice();
    void scanForDevice();

    class AdvertisedDeviceCallbacks : public BLEAdvertisedDeviceCallbacks {
    public:
        TTRemoteInput *enclosed;

        AdvertisedDeviceCallbacks(TTRemoteInput *enclosure) { enclosed = enclosure; }

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

    AdvertisedDeviceCallbacks advertisedDeviceCallbacks_;

    class ClientCallbacks : public BLEClientCallbacks {
    public:
        TTRemoteInput *enclosed;

        ClientCallbacks(TTRemoteInput *enclosure) { enclosed = enclosure; }

        void onConnect(BLEClient *client) override {
            log_d("BLE PageTurner connected!");
            enclosed->state_ = CONNECTED;
        }

        void onDisconnect(BLEClient *client) override {
            log_w("BLE PageTurner disconnected!");
            enclosed->state_ = SCANNING;
        }
    };

    ClientCallbacks clientCallbacks_;
};

#endif
