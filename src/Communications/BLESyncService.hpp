#pragma once

#include <NimBLEDevice.h>

#include "Communications/BLEAbstractions.hpp"

class BLESyncService : public BLEServiceProvider {

public:
    BLESyncService() = default;
    BLESyncService(BLESyncService const &) = delete;
    auto configureService(BLEServer *server) -> BLEService * override;
    auto disableService(BLEServer *server) -> BLEService *;
    auto start() -> bool override;
};
