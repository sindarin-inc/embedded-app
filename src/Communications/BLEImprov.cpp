#if USE_BLE_SERVER_IMPROV

#include "BLEImprov.hpp"

#include "sindarin-debug.h"

void BLEImprov::configureCharacteristics(BLEService *service) {
    auto *characteristicCallbacks = new CharacteristicCallbacks(this);

    BLECharacteristic *capabilitiesCharacteristic =
        service->createCharacteristic(BLEUUID(improv::CAPABILITIES_UUID), NIMBLE_PROPERTY::READ);
    int supportsIdentify = 0;
    capabilitiesCharacteristic->setValue(supportsIdentify);

    BLECharacteristic *currentStateCharacteristic = service->createCharacteristic(
        BLEUUID(improv::STATUS_UUID), NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY);
    int state = improv::STATE_AUTHORIZED;
    currentStateCharacteristic->setValue(state);
    this->currentStateCharacteristic_ = currentStateCharacteristic;

    BLECharacteristic *errorStateCharacteristic = service->createCharacteristic(
        BLEUUID(improv::ERROR_UUID), NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY);
    int noError = improv::ERROR_NONE;
    errorStateCharacteristic->setValue(noError);
    this->errorStateCharacteristic_ = errorStateCharacteristic;

    BLECharacteristic *rpcCommandCharacteristic =
        service->createCharacteristic(BLEUUID(improv::RPC_COMMAND_UUID), NIMBLE_PROPERTY::WRITE);
    rpcCommandCharacteristic->setCallbacks(characteristicCallbacks);

    BLECharacteristic *rpcResultCharacteristic = service->createCharacteristic(
        BLEUUID(improv::RPC_RESULT_UUID), NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY);
    this->rpcResultCharacteristic_ = rpcResultCharacteristic;
}

auto BLEImprov::configureService(BLEServer *server) -> BLEService * {
    service_ = server->createService(BLEUUID(improv::SERVICE_UUID));
    server->getAdvertising()->addServiceUUID(service_->getUUID());
    configureCharacteristics(service_);
    return service_;
}

void BLEImprov::start() {
    service_->start();
    SHOW_HEAP();
}

void BLEImprov::setState(BLEImprovState state) {
    if (this->state_ == state) {
        return;
    }
    this->state_ = state;
    // Translate BLEImprov state to the most relevant Improv state
    int improvState;
    if (state == STATE_WAITING) {
        improvState = improv::STATE_AUTHORIZED;
    } else if (state == STATE_READY_TO_CONNECT || state == STATE_CONNECTING) {
        this->setError(improv::ERROR_NONE, false);
        improvState = improv::STATE_PROVISIONING;
    } else if (state == STATE_CONNECTED) {
        this->setError(improv::ERROR_NONE, false);
        improvState = improv::STATE_PROVISIONED;
    }
    log_d("Switched improv state to %d (internal state = %d)", improvState, state);
#if !defined(DISABLE_IMPROV)
    this->currentStateCharacteristic_->setValue(improvState);
    this->currentStateCharacteristic_->notify();
#endif
    this->lastStateChangeMillis_ = millis();
}

void BLEImprov::setError(improv::Error error, bool notify) {
#if !defined(DISABLE_IMPROV)
    int errorInt = error;
    this->errorStateCharacteristic_->setValue(errorInt);
    if (notify) {
        this->errorStateCharacteristic_->notify();
    }
#endif
}

void BLEImprov::doneConnecting() {
    this->setState(STATE_CONNECTED);
    log_i("Connected, IP address: %s", WiFi.localIP().toString().c_str());
    SHOW_HEAP();
}

void BLEImprov::loop() {
    if (this->state_ == STATE_WAITING) {
        if (WiFi.isConnected()) {
            this->doneConnecting();
        }
    } else if (this->state_ == STATE_READY_TO_CONNECT) {
        improv::ImprovCommand command = this->lastCommand_;
        log_d("Attempting connection to: %s Password: %s (WiFi status = %d)", command.ssid.c_str(),
              command.password.c_str(), WiFi.status());
        WiFi.begin(command.ssid.c_str(), command.password.c_str());
        this->setState(STATE_CONNECTING);
        return;
    } else if (this->state_ == STATE_CONNECTING) {
        uint32_t now = millis();
        if (WiFi.status() == WL_CONNECTED) {
            std::string ip = WiFi.localIP().toString().c_str();
            std::vector<uint8_t> response = improv::build_rpc_response(improv::WIFI_SETTINGS, {ip});
            log_d("Reporting RPC result to client");
            this->rpcResultCharacteristic_->setValue(response.data(), response.size());
            this->rpcResultCharacteristic_->notify();
            this->doneConnecting();
        } else if (now - this->lastStateChangeMillis_ > 30000) {
            log_w("Timed out connecting to WiFi %d", WiFi.status());
            this->setState(STATE_WAITING);
            this->setError(improv::ERROR_UNABLE_TO_CONNECT);
            WiFi.disconnect();
        }
    } else if (this->state_ == STATE_CONNECTED) {
        if (!WiFi.isConnected()) {
            this->setState(STATE_WAITING);
        }
    }
}

#endif
