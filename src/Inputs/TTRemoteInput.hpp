#ifndef TTRemoteInput_H
#define TTRemoteInput_H

#include <NimBLEDevice.h>

#include "Communications/BLEManager.hpp"
#include "RemoteInput.hpp"
#include "sindarin-debug.h"

#define RESCAN_INTERVAL_MS 3000
#define TURNTOUCH_BUTTON_SVC_ID "1523"
#define TURNTOUCH_BUTTON_SVC_UUID "99c31523-dc4f-41b1-bb04-4e4deb81fadd"
#define TURNTOUCH_BUTTON_CHR_UUID "99c31525-dc4f-41b1-bb04-4e4deb81fadd"
#define TT_NONE 0xFF00
#define TT_N_PRESS 0xFE00
#define TT_N_DOUBLE 0xEF00
#define TT_N_HOLD 0xFEFF
#define TT_S_PRESS 0xF700
#define TT_S_DOUBLE 0x7F00
#define TT_S_HOLD 0xF7FF
#define TT_E_PRESS 0xFD00
#define TT_E_DOUBLE 0xDF00
#define TT_E_HOLD 0xFDFF
#define TT_W_PRESS 0xFB00
#define TT_W_DOUBLE 0xBF00
#define TT_W_HOLD 0xFBFF
#define TT_MULTI_N_S 0xF600

class TTRemoteInput : public RemoteInput {

public:
    void start_remote(int receive_pin_input) override;
    void start_remote() override;
    auto check_press() -> int override;
    auto check_release() -> int override;
    void buttonNotificationCallback(BLERemoteCharacteristic *remoteCharacteristic, uint8_t *data,
                                    size_t length, bool isNotify);

protected:
    BLEAdvertisedDevice *turntouchDevice = nullptr;
    bool deviceConnected, wasDeviceConnected;
    int was_code_processed;
    RemoteCommand ble_code;
    long lastScanMs = 0;

    void connectToDevice(BLEAdvertisedDevice *device);
    void scanForDevice();

    class AdvertisedDeviceCallbacks : public BLEAdvertisedDeviceCallbacks {
    public:
        TTRemoteInput *enclosed;

        AdvertisedDeviceCallbacks(TTRemoteInput *enclosure) { enclosed = enclosure; }
        void onResult(BLEAdvertisedDevice *advertisedDevice) override {
            // DEBUG_F("BLEAdvertisedDevice found: %s\n", advertisedDevice.getName().c_str());
            // DEBUG_LN(advertisedDevice.toString().c_str());

            if (advertisedDevice->haveServiceUUID()) {
                DEBUG_F("BLEAdvertisedDevice w/ services found: %s\n",
                        advertisedDevice->getName().c_str());
                for (int i = 0; i < advertisedDevice->getServiceUUIDCount(); ++i) {
                    DEBUG_F("Service %u - %s\n", i,
                            advertisedDevice->getServiceUUID(i).toString().c_str());
                }
                // DEBUG_LN(advertisedDevice.toString().c_str());
                if (advertisedDevice->isAdvertisingService(BLEUUID(TURNTOUCH_BUTTON_SVC_ID))) {
                    BLEDevice::getScan()->stop();
                    DEBUG_LN("Matching TurnTouch device found; queueing for connect...");
                    if (enclosed->turntouchDevice != nullptr) {
                        delete enclosed->turntouchDevice;
                    }
                    enclosed->turntouchDevice = new BLEAdvertisedDevice(*advertisedDevice);
                }
            }
        }
    };

    class ClientCallbacks : public BLEClientCallbacks {
    public:
        TTRemoteInput *enclosed;

        ClientCallbacks(TTRemoteInput *enclosure) { enclosed = enclosure; }

        void onConnect(BLEClient *client) override {
            DEBUG_LN("Our BLE client connected to matching server!");
        }

        void onDisconnect(BLEClient *client) override {
            DEBUG_LN("Our BLE client disconnected from matching server!");
            enclosed->deviceConnected = false;
        }
    };

    class CharacteristicCallbacks : public BLECharacteristicCallbacks {
    public:
        TTRemoteInput *enclosed;

        CharacteristicCallbacks(TTRemoteInput *enclosure) { enclosed = enclosure; }

        void onWrite(BLECharacteristic *characteristic) override {
            std::string value = characteristic->getValue();
            if (value.length() > 0) {
                DEBUG_LN("*********");
                DEBUG_F("New value: %.2x \n", value[0]);
                DEBUG_LN("*********");
                enclosed->ble_code = (RemoteCommand)(uint8_t)value.data()[0];
                enclosed->was_code_processed = 0;
            }
        }
    };
};

#endif