#pragma once

#include <NimBLEDevice.h>

#define MTU_CHUNK_SIZE 512

//! Abstract class that can add one or more BLECharacteristics to a BLEService
class BLECharacteristicProvider {
public:
    virtual void configureCharacteristics(BLEService *service) = 0;
};

//! Abstract class that provides a BLEService
class BLEServiceProvider {
protected:
    BLEService *service_;

public:
    auto getService() -> BLEService * { return service_; };
    virtual auto configureService(BLEServer *server) -> BLEService * = 0;
    virtual auto start() -> bool = 0;
};
