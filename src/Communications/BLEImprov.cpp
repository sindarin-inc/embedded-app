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
    switch (state) {
    case STATE_WAITING:
        improvState = improv::STATE_AUTHORIZED;
        break;
    case STATE_READY_TO_CONNECT:
    case STATE_CONNECTING:
        this->setError(improv::ERROR_NONE, false);
        improvState = improv::STATE_PROVISIONING;
        break;
    case STATE_CONNECTED:
        this->setError(improv::ERROR_NONE, false);
        improvState = improv::STATE_PROVISIONED;
        break;
    case STATE_READY_TO_SCAN:
    case STATE_SCANNING:
        this->setError(improv::ERROR_NONE, false);
        improvState = improv::STATE_SCANNING;
        break;
    case STATE_SCAN_COMPLETE:
        this->setError(improv::ERROR_NONE, false);
        improvState = improv::STATE_AUTHORIZED;
        break;
    }
    log_d("Switched improv state to %d (internal state = %d)", improvState, state);
    SHOW_HEAP();
#if !defined(DISABLE_IMPROV)
    this->currentStateCharacteristic_->setValue(improvState);
    this->currentStateCharacteristic_->notify();
#endif
    this->lastStateChangeMillis_ = Millis();
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
    log_i("Connected, IP address: %s", WiFi::getInstance().getLocalIpStr().c_str());
    SHOW_HEAP();
}

// TODO: Would be good to make this event driven
void BLEImprov::loop() {
    if (this->state_ == STATE_WAITING) {
        if (WiFi::getInstance().connected()) {
            this->doneConnecting();
        }
    } else if (this->state_ == STATE_READY_TO_CONNECT) {
        SHOW_HEAP();
        improv::ImprovCommand command = this->lastCommand_;
        log_d("Attempting connection to: %s Password: %s (WiFi connnected = %d)",
              command.ssid.c_str(), command.password.c_str(), WiFi::getInstance().connected());
        WiFi::getInstance().connectWiFi(command.ssid, command.password);
        this->setState(STATE_CONNECTING);
        return;
    } else if (this->state_ == STATE_CONNECTING) {
        uint32_t now = Millis();
        if (WiFi::getInstance().connected()) {
            std::string ip = WiFi::getInstance().getLocalIpStr();
            std::vector<uint8_t> response = improv::build_rpc_response(improv::WIFI_SETTINGS, {ip});
            log_d("Reporting RPC result to client");
            this->rpcResultCharacteristic_->setValue(response.data(), response.size());
            this->rpcResultCharacteristic_->notify();
            this->doneConnecting();
        } else if (WiFi::getInstance().state() == DISCONNECTED) {
            if (WiFi::getInstance().retryable && now - this->lastStateChangeMillis_ < 30000) {
                // Try again
                improv::ImprovCommand command = this->lastCommand_;
                log_d("Retrying connection to: %s Password: %s (WiFi connnected = %d)",
                      command.ssid.c_str(), command.password.c_str(),
                      WiFi::getInstance().connected());
                WiFi::getInstance().connectWiFi(command.ssid, command.password);
            } else {
                log_w("Failed to connecting to WiFi");
                this->setState(STATE_WAITING);
                this->setError(improv::ERROR_UNABLE_TO_CONNECT);
                WiFi::getInstance().disconnectWiFi();
            }
        }
    } else if (this->state_ == STATE_READY_TO_SCAN) {
        this->setState(STATE_SCANNING);

        std::vector<std::string> existingSsidList;

        // TODO: Make this send APs in real time to the app
        int err = WiFi::getInstance().scanNetworks(
            [this, &existingSsidList](const std::shared_ptr<wifi_ap_record_t[]> &apRecords,
                                      int recordCount) {
            log_d("Wifi scanning complete: %d APs found.", recordCount);

            // Sort records in appRecords by rssi
            std::sort(apRecords.get(), apRecords.get() + recordCount,
                      [](const wifi_ap_record_t &a, const wifi_ap_record_t &b) {
                return a.rssi > b.rssi;
            });

            for (int i = 0; i < recordCount; i++) {
                auto record = apRecords.get()[i];

                std::string ssid = std::string(reinterpret_cast<const char *>(record.ssid));
                std::string rssi = std::to_string(record.rssi);
                std::string auth = std::string(record.authmode == WIFI_AUTH_OPEN ? "NO" : "YES");
                std::vector<uint8_t> response =
                    improv::build_rpc_response(improv::GET_WIFI_NETWORKS, {ssid, rssi, auth});

                // Skip duplicate APs that have the same SSID, probably satellite
                // routers/repeaters
                if (std::find(existingSsidList.begin(), existingSsidList.end(), ssid) ==
                    existingSsidList.end()) {
                    existingSsidList.push_back(ssid);
                } else {
                    log_d("Skipping duplicate AP scan result: %s %s %s", ssid.c_str(), rssi.c_str(),
                          auth.c_str());
                    continue;
                }

                if (ssid.length() > 0) {
                    // TODO: This sometimes seems a little lossy and misses some packets
                    log_d("Sending latest AP scan result to client: %s %s %s", ssid.c_str(),
                          rssi.c_str(), auth.c_str());
                    this->rpcResultCharacteristic_->setValue(response.data(), response.size());
                    this->rpcResultCharacteristic_->notify();
                } else {
                    log_d("Skipping empty AP scan result: %s", ssid.c_str());
                }
            }
        });

        this->setState(STATE_SCAN_COMPLETE);

        if (err != ESP_OK) {
            log_e("Error scanning WiFi networks: %s", esp_err_to_name(err));
            this->setError(improv::ERROR_UNKNOWN);
            this->setState(STATE_WAITING);
        }
    } else if (this->state_ == STATE_SCANNING) {
        log_w("WiFi scan running...");
    } else if (this->state_ == STATE_SCAN_COMPLETE) {
        SHOW_HEAP();
        this->setState(STATE_WAITING);
    } else if (this->state_ == STATE_CONNECTED) {
        if (!WiFi::getInstance().connected()) {
            this->setState(STATE_WAITING);
        }
    }
}

#endif
