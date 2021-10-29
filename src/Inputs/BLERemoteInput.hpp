#ifndef BLERemoteInput_H
#define BLERemoteInput_H

#include <NimBLEDevice.h>

#include "Communications/BLEManager.hpp"
#include "RemoteInput.hpp"
#include "sindarin-debug.h"

class BLERemoteInput : public RemoteInput {

public:
    void start_remote(int receive_pin_input) override;
    void start_remote() override;
    auto check_press() -> int override;
    auto check_release() -> int override;

protected:
    bool wasDeviceConnected;
    bool deviceConnected;
    int was_code_processed;
    RemoteCommand ble_code;

    class ServerCallbacks : public BLEServerCallbacks {
    public:
        ServerCallbacks(BLERemoteInput *enclosure) { enclosed = enclosure; }

        BLERemoteInput *enclosed;

        void onConnect(BLEServer *server) override {
            DEBUG_LN("BLE client connected to our server!");
            enclosed->deviceConnected = true;
        }

        void onDisconnect(BLEServer *server) override {
            DEBUG_LN("BLE client disconnected from our server!");
            enclosed->deviceConnected = false;
        }
    };

    class CharacteristicCallbacks : public BLECharacteristicCallbacks {
    public:
        BLERemoteInput *enclosed;

        CharacteristicCallbacks(BLERemoteInput *enclosure) { enclosed = enclosure; }

        void onWrite(BLECharacteristic *characteristic) override {
            std::string value = characteristic->getValue();
            if (value.length() > 0) {
                DEBUG_LN("*********");
                DEBUG_F("New value: %.2x \n", value[0]);
                DEBUG_LN("*********");
                enclosed->ble_code = (RemoteCommand)value.data()[0];
                enclosed->was_code_processed = 0;
            }
        }
    };
};

#endif