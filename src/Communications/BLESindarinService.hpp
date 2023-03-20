#ifndef BLESindarinService_H
#define BLESindarinService_H

#include <NimBLEDevice.h>

#include "Communications/BLEAbstractions.hpp"

//! Stubbed service used as a container for other Sindarin services
class BLESindarinService : public BLEServiceProvider {

public:
    BLESindarinService() = default;
    BLESindarinService(BLESindarinService const &) = delete;
    auto configureService(BLEServer *server) -> BLEService * override;
    auto start() -> bool override;
};

#endif
