#if USE_BLE_SERVER_PROXY

#include "BLESyncService.hpp"

#define SYNC_SERVICE_UUID "db65598c-04b1-469f-833f-c0497d1fac56"

auto BLESyncService::configureService(BLEServer *server) -> BLEService * {
    service_ = server->createService(BLEUUID(SYNC_SERVICE_UUID));
    // server->getAdvertising()->addServiceUUID(service_->getUUID()); // Deferred until syncing
    return service_;
};

auto BLESyncService::disableService(BLEServer *server) -> BLEService * { return service_; };

auto BLESyncService::start() -> bool { return service_->start(); }

#endif
