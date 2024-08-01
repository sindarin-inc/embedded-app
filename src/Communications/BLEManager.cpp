#if CONFIG_USE_BLE_SERVER_REMOTE

#include "BLEManager.hpp"

#include "Communications/BLEImprov.hpp"
#include "Communications/BLEProxy.hpp"
#include "Communications/BLESyncService.hpp"
#include "Inputs/BLERemoteInput.hpp"
#include "Storage/PreferencesStore.hpp"
#include "config.h"

std::shared_ptr<BLEManager> BLEManager::instance = nullptr;

auto BLEManager::getInstance(bool create) -> std::shared_ptr<BLEManager> {
    if (instance == nullptr && create) {
        instance = std::make_shared<BLEManager>();
    }
    return instance;
}

BLEManager::BLEManager() = default;

auto BLEManager::server() -> BLEServer * {
    if (server_ == nullptr) {
        log_d("Creating BLE server");
        server_ = BLEDevice::createServer();
        log_d("Setting BLE callbacks");
        server_->setCallbacks(new ServerCallbacks(this));
    }
    return server_;
}

#if CONFIG_USE_BLE_SERVER_PROXY
auto BLEManager::connectProxy(const std::shared_ptr<CancelFlag> &cancel) -> bool {
    log_i("Connecting to BLE proxy");

    if (connectedProxy) {
        log_i("BLE proxy already connected");
        return true;
    }

    auto timeoutMs = 30000;  // 30 seconds of advertising to connect to BLE proxy on mobile app
    auto logDelayMs = 10000; // 10 seconds between BLE wait logs

    stopAdvertising();
    disableService(BLERemoteInput::getInstance()->getService());
    disableService(BLEImprov::getInstance()->getService());
    enableService(BLEProxy::getInstance()->getService());
    startAdvertisingIfNecessary();

    BLEProxy::getInstance()->registerSubscriptionStatusCallback([this](bool connected) {
        log_i("BLE proxy subscription status now: %d", connected);
        this->connectedProxy = connected;
    });

    // If not connected, wait up to 30s to ensure BLE is connected. If not, bail.
    auto startTime = esp_timer_get_time();
    auto lastLogTime = esp_timer_get_time();
    while (!connectedProxy) {
        if (esp_timer_get_time() - startTime > static_cast<long long>(timeoutMs) * 1000) {
            log_e("Timed out waiting for BLE proxy to connect");
            return false;
        }

        if (esp_timer_get_time() - lastLogTime > (static_cast<long long>(logDelayMs) * 1000)) {
            log_d("Waiting for BLE proxy to connect...");
            lastLogTime = esp_timer_get_time();
        }

        vTaskDelay(1000 / portTICK_PERIOD_MS);

        if (CancelFlag::isCancelled(cancel)) {
            log_d("Cancelled BLE connection");
            return false;
        }
    }

    startTime = esp_timer_get_time();
    lastLogTime = esp_timer_get_time();
    while (!BLEManager::getInstance()->isAuthed()) {
        if (esp_timer_get_time() - startTime > static_cast<long long>(timeoutMs) * 1000) {
            log_e("Timed out waiting for BLE proxy to auth");
            return false;
        }

        if (esp_timer_get_time() - lastLogTime > (static_cast<long long>(logDelayMs) * 1000)) {
            log_d("Waiting for BLE proxy to auth...");
            lastLogTime = esp_timer_get_time();
        }

        vTaskDelay(1000 / portTICK_PERIOD_MS);

        if (CancelFlag::isCancelled(cancel)) {
            log_d("Cancelled BLE connection while waiting for auth");
            return false;
        }
    }

    return true;
}

void BLEManager::disconnectProxy() {
    stopAdvertising();
    disableService(BLEProxy::getInstance()->getService());
    enableService(BLERemoteInput::getInstance()->getService());
    enableService(BLEImprov::getInstance()->getService());
    if (connectedProxy) {
        // Disconnect from proxy
        BLEProxy::getInstance()->disconnect(connId_);
    }

    connectedProxy = false;
}
#endif

auto BLEManager::startAdvertisingIfNecessary() -> bool {
    if (server_ != nullptr) {
        auto advertising = server_->getAdvertising();

        // Apple has very specific requirements for the allowed values here and warns:
        // > These specific intervals are the ones to be used exactly! Even the slightest deviation
        // > from these actual intervals may dramatically increase your time to discovery.
        // See:
        // https://developer.apple.com/library/archive/qa/qa1931/_index.html
        // https://developer.apple.com/accessories/Accessory-Design-Guidelines.pdf
        if (PreferencesStore::paired == PairedStatus::Unpaired) {
            log_d("Unpaired glasses advertising");
            // Unpaired glasses advertise more frequently to aide in discovery during onboarding.
            // Max interval: 318.75ms. 318.75/0.625=510.
            // This still allows for ~90% (vs 80% with defaults) light sleep time when advertising.
            advertising->setMaxInterval(510);
            // Min interval: 20ms. 20/0.625=32.
            advertising->setMinInterval(32);
        } else {
            log_d("Paired glasses advertising");
            // Paired glasses default, we're using pretty wide intervals since we are typically not
            // connected to the app and we want to maximize battery life.
            // Max interval: 1285ms. 1285/0.625=2056. This is the maximum interval allowed by Apple.
            advertising->setMaxInterval(2056);
            // Min interval: 546.25ms. 546.25/0.625=874. This is a middling interval allowed by
            // Apple.
            advertising->setMinInterval(874);
        }

        log_i("Starting BLE advertising");
        return advertising->start();
    }

    log_i("No BLE server to start advertising");
    return false;
}

void BLEManager::enableService(BLEService *service) {
    if (service != nullptr) {
        server_->getAdvertising()->addServiceUUID(service->getUUID());
    }
}

void BLEManager::disableService(BLEService *service) {
    if (service != nullptr) {
        server_->getAdvertising()->removeServiceUUID(service->getUUID());
    }
}

auto BLEManager::stopAdvertising() -> bool {
    if (server_ != nullptr) {
        log_i("Stopping BLE advertising");
        auto advertising = server_->getAdvertising();
        return advertising->stop();
    }
    log_i("No BLE server to stop advertising");
    return false;
}

#endif
