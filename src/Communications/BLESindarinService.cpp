#if USE_BLE_SERVER_REMOTE || USE_BLE_SERVER_SETTINGS

#include "BLESindarinService.hpp"

#define SERVICE_UUID "db65598c-04b1-469f-833f-517f285db256"

auto BLESindarinService::configureService(BLEServer *server) -> BLEService * {
    service_ = server->createService(BLEUUID(SERVICE_UUID));
    server->getAdvertising()->addServiceUUID(service_->getUUID());
    return service_;
};

void BLESindarinService::start() { service_->start(); }

#endif