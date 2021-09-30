
#include "BLEWifi.hpp"
#include "sindarin-debug.h"
#include <Arduino.h>
#include <NimBLEDevice.h>
#include "esp_wifi.h"

void BLEWifi::startService() {
  DEBUG_LN("BLEWifi::startService");
  SHOW_HEAP();

  BLEServer *server = BLEManager::getInstance()->server;

  // NOTE(john): It might make sense at some point to consolidate everything
  // as one service. However keeping these services separate for now allows us
  // to use the Improv WebBluetooth implementation for Wifi provisioning and
  // also use the Sindarin mobile app as-is after provisioning is done.
  service = server->createService(BLEUUID(improv::SERVICE_UUID));

  CharacteristicCallbacks *characteristic_callbacks = new CharacteristicCallbacks(this);

  BLECharacteristic *capabilities_characteristic = service->createCharacteristic(
      BLEUUID(improv::CAPABILITIES_UUID),
      NIMBLE_PROPERTY::READ);
  int supports_identify = 0;
  capabilities_characteristic->setValue(supports_identify);

  BLECharacteristic *current_state_characteristic = service->createCharacteristic(
      BLEUUID(improv::STATUS_UUID),
      NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY);
  int state = improv::STATE_AUTHORIZED;
  current_state_characteristic->setValue(state);
  this->current_state_characteristic = current_state_characteristic;

  BLECharacteristic *error_state_characteristic = service->createCharacteristic(
      BLEUUID(improv::ERROR_UUID),
      NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY);
  int no_error = improv::ERROR_NONE;
  error_state_characteristic->setValue(no_error);
  this->error_state_characteristic = error_state_characteristic;

  BLECharacteristic *rpc_command_characteristic = service->createCharacteristic(
      BLEUUID(improv::RPC_COMMAND_UUID),
      NIMBLE_PROPERTY::WRITE);
  rpc_command_characteristic->setCallbacks(characteristic_callbacks);

  BLECharacteristic *rpc_result_characteristic = service->createCharacteristic(
      BLEUUID(improv::RPC_RESULT_UUID),
      NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY);
  this->rpc_result_characteristic = rpc_result_characteristic;

  service->start();
  BLEAdvertising *advertising = server->getAdvertising();
  advertising->addServiceUUID(service->getUUID());
  SHOW_HEAP();

  // If credentials are available, go ahead and attempt WiFi connection
  if(!WiFi.enableSTA(true)) {
    DEBUG_F("STA enable failed!");
  }
  wifi_config_t conf;
  esp_wifi_get_config(WIFI_IF_STA, &conf);
  const char *ssid = (const char *)conf.sta.ssid;
  const char *password = (const char *)conf.sta.password;
  if (strlen(ssid) > 0 && strlen(password) > 0) {
    DEBUG_F("Found WiFi credentials for %s:%s. Attempting connection...\n", ssid, password);
    WiFi.begin();
  }
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

void BLEWifi::doneConnecting() {
  this->setState(STATE_CONNECTED);
  DEBUG_LN("Connected, IP address: ");
  DEBUG_LN(WiFi.localIP());
  SHOW_HEAP();
  // Now that we're connected, we may want to close the Improv service
  DEBUG_LN("Closing down Improv service: ");
  BLEServer *server = BLEManager::getInstance()->server;
  server->removeService(service, true);
  service = NULL;
  // Not exactly sure why, but it seems necessary to restart other Bluetooth services
  // after stopping this one.
  BLEManager::getInstance()->startServiceAndAdvertising();
  SHOW_HEAP();
}

void BLEWifi::loop() {
  if (this->state == STATE_WAITING) {
    if (WiFi.isConnected()) {
      this->doneConnecting();
    }
  } else if (this->state == STATE_READY_TO_CONNECT) {
    improv::ImprovCommand command = this->lastCommand;
    DEBUG_F("Attempting connection to: %s Password: %s (WiFi status = %d)\n", command.ssid.c_str(), command.password.c_str(), WiFi.status());
    WiFi.begin(command.ssid.c_str(), command.password.c_str());
    this->setState(STATE_CONNECTING);
    return;
  } else if (this->state == STATE_CONNECTING) {
    uint32_t now = millis();
    if (WiFi.status() == WL_CONNECTED) {
      std::string ip = WiFi.localIP().toString().c_str();
      std::vector<uint8_t> response = improv::build_rpc_response(improv::WIFI_SETTINGS, {ip});
      log_d("Reporting RPC result to client");
      this->rpc_result_characteristic->setValue(response.data(), response.size());
      this->rpc_result_characteristic->notify();
      this->doneConnecting();
    } else if (now - this->last_state_change_millis > 30000) {
      DEBUG_F("Timed out connecting to WiFi %d\n", WiFi.status());
      this->setState(STATE_WAITING);
      this->setError(improv::ERROR_UNABLE_TO_CONNECT);
      WiFi.disconnect();
    }
  } else if (this->state == STATE_CONNECTED) {
    if (!WiFi.isConnected()) {
      this->setState(STATE_WAITING);
      this->startService();
    }
  }
}
