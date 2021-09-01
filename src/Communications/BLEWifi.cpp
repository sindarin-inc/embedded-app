
#include "BLEWifi.hpp"
#include "sindarin-debug.h"
#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEService.h>
#include <BLE2902.h>
#include <BLEDescriptor.h>
#include "esp_wifi.h"

void BLEWifi::startService() {
  DEBUG_LN("BLEWifi::startService");

  // TODO(john): I'm not sure why, but calling WiFi.begin before starting Bluetooth
  // seems to make WiFi work just a little bit better. I wonder if there's something
  // about the WiFi/BLE coexistence that likes to have the WiFi enabled first. More
  // research is needed To make WiFi reliable here.
  WiFi.begin();

  BLEServer *server = BLEManager::getInstance()->server;

  // NOTE(john): It might make sense at some point to consolidate everything
  // as one service. However keeping these services separate for now allows us
  // to use the Improv WebBluetooth implementation for Wifi provisioning and
  // also use the Sindarin mobile app as-is after provisioning is done.
  BLEService *service = server->createService(BLEUUID(improv::SERVICE_UUID));
  server->setCallbacks(new ServerCallbacks(this));

  CharacteristicCallbacks *characteristic_callbacks = new CharacteristicCallbacks(this);

  BLECharacteristic *capabilities_characteristic = service->createCharacteristic(
      BLEUUID(improv::CAPABILITIES_UUID),
      BLECharacteristic::PROPERTY_READ);
  BLEDescriptor *capabilities_descriptor = new BLE2902();
  capabilities_characteristic->addDescriptor(capabilities_descriptor);
  int supports_identify = 0;
  capabilities_characteristic->setValue(supports_identify);

  BLECharacteristic *current_state_characteristic = service->createCharacteristic(
      BLEUUID(improv::STATUS_UUID),
      BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_NOTIFY);
  BLEDescriptor *current_state_descriptor = new BLE2902();
  current_state_characteristic->addDescriptor(current_state_descriptor);
  int state = improv::STATE_AUTHORIZED;
  current_state_characteristic->setValue(state);
  this->current_state_characteristic = current_state_characteristic;

  BLECharacteristic *error_state_characteristic = service->createCharacteristic(
      BLEUUID(improv::ERROR_UUID),
      BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_NOTIFY);
  BLEDescriptor *error_state_descriptor = new BLE2902();
  error_state_characteristic->addDescriptor(error_state_descriptor);
  int no_error = improv::ERROR_NONE;
  error_state_characteristic->setValue(no_error);
  this->error_state_characteristic = error_state_characteristic;

  BLECharacteristic *rpc_command_characteristic = service->createCharacteristic(
      BLEUUID(improv::RPC_COMMAND_UUID),
      BLECharacteristic::PROPERTY_WRITE);
  BLEDescriptor *rpc_command_descriptor = new BLE2902();
  rpc_command_characteristic->addDescriptor(rpc_command_descriptor);
  rpc_command_characteristic->setCallbacks(characteristic_callbacks);

  BLECharacteristic *rpc_result_characteristic = service->createCharacteristic(
      BLEUUID(improv::RPC_RESULT_UUID),
      BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_NOTIFY);
  BLEDescriptor *rpc_result_descriptor = new BLE2902();
  rpc_result_characteristic->addDescriptor(rpc_result_descriptor);
  this->rpc_result_characteristic = rpc_result_characteristic;

  service->start();
  BLEAdvertising *advertising = server->getAdvertising();
  advertising->addServiceUUID(service->getUUID());
}

void BLEWifi::setState(BLEWifiState state) {
  if (this->state == state) {
    return;
  }
  this->state = state;
  // Translate BLEWifi state to the most relevant Improv state
  int improv_state;
  if (state == STATE_WAITING) {
    improv_state = improv::STATE_AUTHORIZED;
  } else if (state == STATE_READY_TO_CONNECT) {
    this->setError(improv::ERROR_NONE, false);
    improv_state = improv::STATE_PROVISIONING;
  } else if (state == STATE_CONNECTING) {
    this->setError(improv::ERROR_NONE, false);
    improv_state = improv::STATE_PROVISIONING;
  } else if (state == STATE_CONNECTED) {
    this->setError(improv::ERROR_NONE, false);
    improv_state = improv::STATE_PROVISIONED;
  }
  this->current_state_characteristic->setValue(improv_state);
  this->current_state_characteristic->notify();
  this->last_state_change_millis = millis();
}

void BLEWifi::setError(improv::Error error, bool notify) {
  int error_int = error;
  this->error_state_characteristic->setValue(error_int);
  if (notify) {
    this->error_state_characteristic->notify();
  }
}

void BLEWifi::loop() {
  if (this->state == STATE_READY_TO_CONNECT) {
    improv::ImprovCommand command = this->lastCommand;
    DEBUG_F("Attempting connection to: %s Password: %s (WiFi status = %d)\n", command.ssid.c_str(), command.password.c_str(), WiFi.status());
    // TODO(john): Store the credentials and reconnect automatically at boot if
    // credentials already exist.
    WiFi.begin(command.ssid.c_str(), command.password.c_str());
    this->setState(STATE_CONNECTING);
    return;
  } else if (this->state == STATE_CONNECTING) {
    uint32_t now = millis();
    if (WiFi.status() == WL_CONNECTED) {
      DEBUG_LN("Connected to WiFi");

      // Optionally return some data to the host app
      // std::string url = WiFi.localIP().toString().c_str();
      // std::vector<uint8_t> response = improv::build_rpc_response(improv::WIFI_SETTINGS, {url});
      // this->rpc_result_characteristic->setValue(response.data(), response.size());
      // this->rpc_result_characteristic->notify();

      this->setState(STATE_CONNECTED);
      DEBUG_LN("Connected, IP address: ");
      DEBUG_LN(WiFi.localIP());
    } else if (now - this->last_state_change_millis > 30000) {
      DEBUG_F("Timed out connecting to WiFi %d\n", WiFi.status());
      this->setState(STATE_WAITING);
      this->setError(improv::ERROR_UNABLE_TO_CONNECT);
      WiFi.disconnect();
    }
  }
}
