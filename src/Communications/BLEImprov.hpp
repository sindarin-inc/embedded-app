#pragma once

#include <NimBLEDevice.h>
#include <esp_event_base.h>
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
    // Received RPC command to scan for WiFi APs
    STATE_READY_TO_SCAN = 0x04,
    // Scanning for WiFi APs
    STATE_SCANNING = 0x05,
    // Done scanning for WiFi APs
    STATE_SCAN_COMPLETE = 0x06,
};

class BLEImprov : public BLECharacteristicProvider, public BLEServiceProvider {
public:
    BLEImprov() : BLECharacteristicProvider(), BLEServiceProvider(){};
    BLEImprov(BLEImprov const &) = delete;
    ~BLEImprov() override = default;

    static auto getInstance() -> std::shared_ptr<BLEImprov>;
    void loop();
    auto getService() -> BLEService * { return service_; };

    void configureCharacteristics(BLEService *service) override;
    auto configureService(BLEServer *server) -> BLEService * override;
    auto start() -> bool override;
    void wifiEventHandler(void *arg, esp_event_base_t eventBase, int32_t eventId, void *eventData);
    void setState(BLEImprovState state);

protected:
    static std::shared_ptr<BLEImprov> instance;
    uint32_t lastStateChangeMillis_{};
    BLECharacteristic *currentStateCharacteristic_{};
    BLECharacteristic *errorStateCharacteristic_{};
    BLECharacteristic *rpcResultCharacteristic_{};
    BLEImprovState state_ = STATE_WAITING;
    improv::ImprovCommand lastCommand_;

    void setError(improv::Error error, bool notify = true);
    void doneConnecting();

    class CharacteristicCallbacks : public BLECharacteristicCallbacks {
    public:
        CharacteristicCallbacks() = default;

        void onWrite(BLECharacteristic *characteristic, ble_gap_conn_desc *desc) override {
            if (!BLEManager::getInstance()->checkAuthed(desc)) {
                return;
            }

            log_d("onWrite for Characteristic %s", characteristic->getUUID().toString().c_str());
            if (characteristic->getUUID().equals(BLEUUID(improv::RPC_COMMAND_UUID))) {
                std::string value = characteristic->getValue();
                std::byte bytes[value.length()];
                memcpy(bytes, value.data(), value.length());

                improv::ImprovCommand command = improv::parse_improv_data(
                    reinterpret_cast<const uint8_t *>(value.c_str()), value.length());
                std::string commandStr;
                for (int i = 0; i < value.length(); i++) {
                    commandStr += std::to_string(static_cast<int>(bytes[i]));
                    commandStr += " ";
                }
                log_d("onWrite for Improv Characteristic %s", commandStr.c_str());
                switch (command.command) {
                case improv::BAD_CHECKSUM: {
                    log_e("Error: Bad Checksum from Improv message");
                    break;
                }
                case improv::WIFI_SETTINGS: {
                    BLEImprov::getInstance()->lastCommand_ = command;
                    BLEImprov::getInstance()->setState(STATE_READY_TO_CONNECT);
                    break;
                }
                case improv::GET_WIFI_NETWORKS: {
                    BLEImprov::getInstance()->lastCommand_ = command;
                    BLEImprov::getInstance()->setState(STATE_READY_TO_SCAN);
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

    CharacteristicCallbacks characteristicCallbacks_;
};
