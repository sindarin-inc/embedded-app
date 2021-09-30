#ifndef BLEWifi_H
#define BLEWifi_H

#include "Communications/BLEManager.hpp"
#include "sindarin-debug.h"

#include <NimBLEDevice.h>
#include <improv.h>
#include "WiFi.h"

enum BLEWifiState : uint8_t {
  // Waiting for credentials
  STATE_WAITING = 0x00,
  // Received WiFi credentials and ready to start connecting
  STATE_READY_TO_CONNECT = 0x01,
  // Attempting to connect to WiFi
  STATE_CONNECTING = 0x02,
  // Connected to WiFi
  STATE_CONNECTED = 0x03,
};

class BLEWifi {
  public:
    void startService();
    void loop();

  protected:
    uint32_t last_state_change_millis;
    BLECharacteristic *current_state_characteristic;
    BLECharacteristic *error_state_characteristic;
    BLECharacteristic *rpc_result_characteristic;
    BLEService *service;
    BLEWifiState state;
    improv::ImprovCommand lastCommand;

    void setState(BLEWifiState state);
    void setError(improv::Error error, bool notify = true);
    void doneConnecting();

    class CharacteristicCallbacks : public BLECharacteristicCallbacks {
      public:
        BLEWifi* enclosed;

        CharacteristicCallbacks(BLEWifi* enclosure) { 
          enclosed = enclosure;
        }

        void onWrite(BLECharacteristic *characteristic) {
          DEBUG_F("onWrite for Characteristic %s\n", characteristic->getUUID().toString().c_str());
          if (characteristic->getUUID().equals(BLEUUID(improv::RPC_COMMAND_UUID))) {
            std::string value = characteristic->getValue();
            improv::ImprovCommand command = improv::parse_improv_data((uint8_t *)value.c_str(), value.length());
            switch (command.command) {
              case improv::BAD_CHECKSUM: {
                DEBUG_LN("Error: Bad Checksum from Improv message");
                break;
              }
              case improv::WIFI_SETTINGS: {
                this->enclosed->lastCommand = command;
                this->enclosed->setState(STATE_READY_TO_CONNECT);
                break;
              }
              default: {
                DEBUG_LN("Error: Unhandled Improv message");
              }
            }
          } else {
            DEBUG_LN("Error: Unhandled characteristic in BLEWifi");
          }
        }
      };
};

#endif
