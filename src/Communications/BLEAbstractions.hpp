#pragma once

#include <NimBLEDevice.h>

#define MTU_CHUNK_SIZE 512
// There are at least a few bytes of overhead per chunk, so we need to reduce the chunk size
#define MTU_REDUCED_CHUNK_SIZE 496

//! Abstract class that can add one or more BLECharacteristics to a BLEService
class BLECharacteristicProvider {
public:
    virtual void configureCharacteristics(BLEService *service) = 0;
    virtual ~BLECharacteristicProvider() = default;
};

//! Abstract class that provides a BLEService
class BLEServiceProvider {
protected:
    BLEService *service_{};

public:
    virtual ~BLEServiceProvider() = default;
    auto getService() -> BLEService * { return service_; };
    virtual auto configureService(BLEServer *server) -> BLEService * = 0;
    virtual auto start() -> bool = 0;
};
