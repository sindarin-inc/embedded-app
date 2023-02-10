#if USE_BLE_SERVER_IMPROV

#include "BLEImprov.hpp"

#include "Communications/WiFi.hpp"
#include "config.h"
#include "esp_wifi.h"
#include "sindarin-debug.h"

// Time to wait for WiFi scanning to complete. Should not trigger if the event handler fires a
// `WIFI_EVENT_SCAN_DONE` event.
#define WIFI_SCANNING_TIMEOUT_MS 5000

// Max number of WiFi APs to scan for.
#define WIFI_SCANNING_MAX_AP_RECORDS 20

void BLEImprov::configureCharacteristics(BLEService *service) {
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
    rpcCommandCharacteristic->setCallbacks(&characteristicCallbacks_);

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

auto BLEImprov::start() -> bool { return service_->start(); }

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
    } else if (state == STATE_SCANNING) {
        this->setError(improv::ERROR_NONE, false);
        improvState = improv::STATE_SCANNING;
    } else {
        this->setError(improv::ERROR_NONE, false);
        improvState = improv::STATE_PROVISIONED;
    }
    log_d("Switched improv state to %d (internal state = %d)", improvState, state);
    SHOW_HEAP();
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

// This event handler is unused due to `esp_event_handler_register` not able to register a class
// method
// void BLEImprov::wifiEventHandler(void *arg, esp_event_base_t eventBase, int32_t eventId,
//                                  void *eventData) {
//     if (eventBase == WIFI_EVENT && eventId == WIFI_EVENT_SCAN_DONE) {
//         log_d("WIFI_EVENT_SCAN_DONE");
//         BLEImprov::setState(STATE_SCAN_COMPLETE);
//     }
// }

// static void eventHandler(void *arg, esp_event_base_t eventBase, int32_t eventId, void *eventData)
// {
//     log_d("WIFI_EVENT_SCAN_DONE? %d", eventId);
//     if (eventBase == WIFI_EVENT && eventId == WIFI_EVENT_SCAN_DONE) {
//         log_d("WIFI_EVENT_SCAN_DONE!");
//         // BLEImprov::setState(STATE_SCAN_COMPLETE);
//     }
// }

// TODO: Would be good to make this event driven
void BLEImprov::loop() {
    if (this->state_ == STATE_WAITING) {
        if (WiFi.isConnected()) {
            this->doneConnecting();
        }
    } else if (this->state_ == STATE_READY_TO_CONNECT) {
        SHOW_HEAP();
        improv::ImprovCommand command = this->lastCommand_;
        log_d("Attempting connection to: %s Password: %s (WiFi status = %d)", command.ssid.c_str(),
              command.password.c_str(), WiFi.status());
        ConnectWiFi(command.ssid, command.password);
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
    } else if (this->state_ == STATE_READY_TO_SCAN) {
        bool asyncScan = true;
        this->lastAPsent_ = 0;

        // ESP IDF method of scanning has been removed but can be found newly commented out at
        // https://github.com/sindarin-inc/embedded-app/commit/131a40ac99ca0be289887eadbe8da8cd1bdc2d45

        // Set WiFi to station mode and disconnect from an AP if it was previously connected
        WiFi.mode(WIFI_STA);
        WiFi.disconnect();

        int err = WiFi.scanNetworks(asyncScan);
        if (err == WIFI_SCAN_RUNNING) {
            this->setState(STATE_SCANNING);
        } else {
            this->setState(STATE_SCAN_COMPLETE);
        }
    } else if (this->state_ == STATE_SCANNING) {
        if (WiFi.scanComplete() == WIFI_SCAN_RUNNING) {
            log_w("WiFi scan running...");
        } else if (WiFi.scanComplete() == WIFI_SCAN_FAILED) {
            log_w("WiFi scan failed");
            this->setState(STATE_SCAN_COMPLETE);
        } else {
            log_d("Wifi scanning: %d APs found.", WiFi.scanComplete());
            this->setState(STATE_SCAN_COMPLETE);
        }
    } else if (this->state_ == STATE_SCAN_COMPLETE) {
        SHOW_HEAP();

        uint16_t scanCount = WiFi.scanComplete();
        std::vector<std::string> ssidList;
        for (int i = 0; i < scanCount; i++) {
            log_d("Wifi scan complete, %d APs found... %d", scanCount, i);
            std::string ssid = std::string(WiFi.SSID(i).c_str());
            std::string rssi = std::to_string(WiFi.RSSI(i));
            std::string auth = std::string(WiFi.encryptionType(i) == WIFI_AUTH_OPEN ? "NO" : "YES");
            std::vector<uint8_t> response =
                improv::build_rpc_response(improv::GET_WIFI_NETWORKS, {ssid, rssi, auth});

            // Skip duplicate APs that have the same SSID, probably satellite routers/repeaters
            if (std::find(ssidList.begin(), ssidList.end(), ssid) == ssidList.end()) {
                ssidList.push_back(ssid);
            } else {
                log_d("Skipping duplicate AP scan result: %s %s %s", ssid.c_str(), rssi.c_str(),
                      auth.c_str());
                continue;
            }

            if (ssid.length() > 0) {
                log_d("Sending latest AP scan result to client: %s %s %s", ssid.c_str(),
                      rssi.c_str(), auth.c_str());
                this->rpcResultCharacteristic_->setValue(response.data(), response.size());
                this->rpcResultCharacteristic_->notify();
            } else {
                log_d("Skipping empty AP scan result: %s", ssid.c_str());
            }
        }
        this->setState(STATE_WAITING);
    } else if (this->state_ == STATE_CONNECTED) {
        if (!WiFi.isConnected()) {
            this->setState(STATE_WAITING);
        }
    }
}

#endif
