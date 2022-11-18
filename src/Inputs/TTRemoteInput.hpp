#ifndef TTRemoteInput_H
#define TTRemoteInput_H

#include <NimBLEDevice.h>

#include "Communications/BLEManager.hpp"
#include "RemoteInput.hpp"
#include "Storage/PreferencesStore.hpp"
#include "TurnTouch.hpp"
#include "sindarin-debug.h"

#define RESCAN_INTERVAL_MS 5000

class TTRemoteInput : public RemoteInput {
public:
    TTRemoteInput(PreferencesStore &prefs) noexcept : prefs_(prefs){};

    // Connect to the TurnTouch. Can be safely called even if the TurnTouch is already connected.
    void connect();
    void disconnectTemporarily(int seconds);

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
    BLEAdvertisedDevice *turntouchDevice_ = nullptr;
    long lastScanMs_ = 0;
    long keepDisconnectedUntilMs_ = 0;
    BLEClient *client_ = nullptr;

    auto connectToDevice(BLEAdvertisedDevice *device) -> bool;
    static void connectTask(void *params);
    void scanForDevice();

    class AdvertisedDeviceCallbacks : public BLEAdvertisedDeviceCallbacks {
    public:
        TTRemoteInput *enclosed;

        AdvertisedDeviceCallbacks(TTRemoteInput *enclosure) { enclosed = enclosure; }

        void onResult(BLEAdvertisedDevice *advertisedDevice) override {
            if (advertisedDevice->haveServiceUUID()) {
                log_v("BLEAdvertisedDevice w/ services found: %s",
                      advertisedDevice->getName().c_str());
                for (int i = 0; i < advertisedDevice->getServiceUUIDCount(); ++i) {
                    log_v("Service %u - %s", i,
                          advertisedDevice->getServiceUUID(i).toString().c_str());
                }
                // Connect to device if:
                //  - This is the paired device
                //  - We don't have any paired devices and this device is advertising the TurnTouch
                //    button service
                std::optional<BLEAddress> address = enclosed->prefs_.pairedTurnTouchAddress.get();
                if ((address.has_value() && address.value() == advertisedDevice->getAddress()) ||
                    (!address.has_value() &&
                     advertisedDevice->isAdvertisingService(BLEUUID(TURNTOUCH_BUTTON_SVC_ID)))) {
                    enclosed->state_ = CONNECTING;
                    BLEDevice::getScan()->stop();
                    if (enclosed->turntouchDevice_ != nullptr) {
                        delete enclosed->turntouchDevice_;
                    }
                    enclosed->turntouchDevice_ = new BLEAdvertisedDevice(*advertisedDevice);
                    log_i("%s TurnTouch device found; queueing for connect...",
                          address.has_value() ? "Paired" : "New");
                    // Fire off a high-ish priority temporary task to connect the TT
                    xTaskCreatePinnedToCore(this->enclosed->connectTask, "TTConnect", 10000,
                                            this->enclosed, 2, nullptr, 0);
                }
            }
        }
    };

    class ClientCallbacks : public BLEClientCallbacks {
    public:
        TTRemoteInput *enclosed;

        ClientCallbacks(TTRemoteInput *enclosure) { enclosed = enclosure; }

        void onConnect(BLEClient *client) override {
            log_d("BLE TurnTouch client connected!");
            enclosed->state_ = CONNECTED;
        }

        void onDisconnect(BLEClient *client) override {
            log_w("BLE TurnTouch client disconnected!");
            enclosed->state_ = SCANNING;
            if (enclosed->turntouchDevice_ != nullptr) {
                delete enclosed->turntouchDevice_;
                enclosed->turntouchDevice_ = nullptr;
            }
        }
    };
};

#endif
