#ifndef BLEImprov_H
#define BLEImprov_H

#include <NimBLEDevice.h>
#include <WiFi.h>
#include <improv.h>

#include "Communications/BLEAbstractions.hpp"
#include "Communications/BLEManager.hpp"
#include "sindarin-debug.h"

enum BLEImprovState : uint8_t {
    // Waiting for credentials
    STATE_WAITING = 0x00,
    // Received WiFi credentials and ready to start connecting
    STATE_READY_TO_CONNECT = 0x01,
    // Attempting to connect to WiFi
    STATE_CONNECTING = 0x02,
    // Connected to WiFi
    STATE_CONNECTED = 0x03,
};

class BLEImprov : public BLECharacteristicProvider, public BLEServiceProvider {
public:
    void loop();

    void configureCharacteristics(BLEService *service) override;
    auto configureService(BLEServer *server) -> BLEService * override;
    void start() override;

protected:
    uint32_t lastStateChangeMillis_;
    BLECharacteristic *currentStateCharacteristic_;
    BLECharacteristic *errorStateCharacteristic_;
    BLECharacteristic *rpcResultCharacteristic_;
    BLEImprovState state_;
    improv::ImprovCommand lastCommand_;

    void setState(BLEImprovState state);
    void setError(improv::Error error, bool notify = true);
    void doneConnecting();
    void startOTA();

    class CharacteristicCallbacks : public BLECharacteristicCallbacks {
    public:
        BLEImprov *enclosed;

        CharacteristicCallbacks(BLEImprov *enclosure) { enclosed = enclosure; }

        void onWrite(BLECharacteristic *characteristic) override {
            log_d("onWrite for Characteristic %s", characteristic->getUUID().toString().c_str());
            if (characteristic->getUUID().equals(BLEUUID(improv::RPC_COMMAND_UUID))) {
                std::string value = characteristic->getValue();
                improv::ImprovCommand command =
                    improv::parse_improv_data((uint8_t *)value.c_str(), value.length());
                switch (command.command) {
                case improv::BAD_CHECKSUM: {
                    log_e("Error: Bad Checksum from Improv message");
                    break;
                }
                case improv::WIFI_SETTINGS: {
                    this->enclosed->lastCommand_ = command;
                    this->enclosed->setState(STATE_READY_TO_CONNECT);
                    break;
                }
                default: {
                    log_e("Error: Unhandled Improv message");
                }
                }
            } else {
                log_e("Error: Unhandled characteristic in BLEImprov");
            }
        }
    };
};

#endif
