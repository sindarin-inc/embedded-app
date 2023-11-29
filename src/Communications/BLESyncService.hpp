#pragma once

#ifndef BLESyncService_H
#define BLESyncService_H

#include <NimBLEDevice.h>

#include "Communications/BLEAbstractions.hpp"

class BLESyncService : public BLEServiceProvider {

public:
    BLESyncService() = default;
    BLESyncService(BLESyncService const &) = delete;
    auto configureService(BLEServer *server) -> BLEService * override;
    auto start() -> bool override;
};

#endif
