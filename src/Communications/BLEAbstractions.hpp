#pragma once

#include <NimBLEDevice.h>

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
    virtual auto configureService(BLEServer *server) -> BLEService * = 0;
    virtual auto start() -> bool = 0;
};
