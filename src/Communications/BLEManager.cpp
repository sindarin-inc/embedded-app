#if USE_BLE_SERVER_REMOTE

#include "BLEManager.hpp"

#include "config.h"

BLEManager *BLEManager::instance = nullptr;

auto BLEManager::getInstance() -> BLEManager * {
    if (instance == nullptr) {
        instance = new BLEManager();
    }
    return instance;
}

BLEManager::BLEManager() = default;

auto BLEManager::server() -> BLEServer * {
    if (server_ == nullptr) {
        server_ = BLEDevice::createServer();
        server_->setCallbacks(new ServerCallbacks());
    }
    return server_;
}

void BLEManager::startAdvertisingIfNecessary() {
    if (server_ != nullptr) {
        auto advertising = server_->getAdvertising();
        advertising->start();
    }
}

#endif
