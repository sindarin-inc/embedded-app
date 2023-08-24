#if USE_BLE_SERVER_REMOTE

#include "BLEManager.hpp"

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
        // We're using pretty wide intervals since we are typically not connected to the app and we
        // want to maximize battery life.
        // Max interval: 1285ms. 1285/0.625=2056. This is the maximum interval allowed by Apple.
        advertising->setMaxInterval(2056);
        // Min interval: 546.25ms. 546.25/0.625=874. This is a middling interval allowed by Apple.
        advertising->setMinInterval(874);
        return advertising->start();
    }
    return false;
}

#endif
