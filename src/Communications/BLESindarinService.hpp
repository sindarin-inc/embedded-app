#pragma once

#include <NimBLEDevice.h>
#include <memory>

#include "Communications/BLEAbstractions.hpp"

//! Stubbed service used as a container for other Sindarin services
class BLESindarinService : public BLEServiceProvider {

public:
    static auto getInstance() -> std::shared_ptr<BLESindarinService>;

    BLESindarinService() = default;
    BLESindarinService(BLESindarinService const &) = delete;
    ~BLESindarinService() override = default;
    auto configureService(BLEServer *server) -> BLEService * override;
    auto start() -> bool override;

protected:
    static std::shared_ptr<BLESindarinService> instance;
};
