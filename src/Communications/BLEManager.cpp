#if USE_BLE_SERVER_REMOTE

#include "BLEManager.hpp"

#include "Storage/PreferencesStore.hpp"
#include "config.h"

std::shared_ptr<BLEManager> BLEManager::instance = nullptr;

auto BLEManager::getInstance() -> std::shared_ptr<BLEManager> {
    if (instance == nullptr) {
        instance = std::make_shared<BLEManager>();
    }
    return instance;
}

BLEManager::BLEManager() = default;

auto BLEManager::server() -> BLEServer * {
    if (server_ == nullptr) {
        server_ = BLEDevice::createServer();
        server_->setCallbacks(new ServerCallbacks(this));
    }
    return server_;
}

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
            // Unpaired glasses advertise more frequently to aide in discovery during onboarding.
            // Max interval: 318.75ms. 318.75/0.625=510.
            // This still allows for ~90% (vs 80% with defaults) light sleep time when advertising.
            advertising->setMaxInterval(510);
            // Min interval: 20ms. 20/0.625=32.
            advertising->setMinInterval(32);
        } else {
            // Paired glasses default, we're using pretty wide intervals since we are typically not
            // connected to the app and we want to maximize battery life.
            // Max interval: 1285ms. 1285/0.625=2056. This is the maximum interval allowed by Apple.
            advertising->setMaxInterval(2056);
            // Min interval: 546.25ms. 546.25/0.625=874. This is a middling interval allowed by
            // Apple.
            advertising->setMinInterval(874);
        }
        return advertising->start();
    }
    return false;
}

void BLEManager::enableService(BLEService *service) {
    server_->getAdvertising()->addServiceUUID(service->getUUID());
}

void BLEManager::disableService(BLEService *service) {
    server_->getAdvertising()->removeServiceUUID(service->getUUID());
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
