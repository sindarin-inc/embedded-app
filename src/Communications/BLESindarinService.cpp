#if CONFIG_USE_BLE_SERVER_REMOTE || CONFIG_USE_BLE_SERVER_SETTINGS

#include "BLESindarinService.hpp"

#define SERVICE_UUID "db65598c-04b1-469f-833f-517f285db256"

std::shared_ptr<BLESindarinService> BLESindarinService::instance = nullptr;

auto BLESindarinService::getInstance() -> std::shared_ptr<BLESindarinService> {
    if (instance == nullptr) {
        instance = std::make_shared<BLESindarinService>();
    }
    return instance;
}

auto BLESindarinService::configureService(BLEServer *server) -> BLEService * {
    service_ = server->createService(BLEUUID(SERVICE_UUID));
    server->getAdvertising()->addServiceUUID(service_->getUUID());
    return service_;
};

auto BLESindarinService::start() -> bool { return service_->start(); }

#endif