#pragma once
#ifndef BLEManager_H
#define BLEManager_H

#include <NimBLEDevice.h>
#include <memory>

#include "sindarin-debug.h"

class BLEManager {
public:
    BLEManager();
    BLEManager(BLEManager const &) = delete;
    static auto getInstance() -> std::shared_ptr<BLEManager>;
    auto startAdvertisingIfNecessary() -> bool;
    auto server() -> BLEServer *;

protected:
    static std::shared_ptr<BLEManager> instance;
    BLEServer *server_ = nullptr;

    class ServerCallbacks : public BLEServerCallbacks {
    public:
        ServerCallbacks() = default;
        void onConnect(BLEServer *server) override { log_i("BLE client connected to our server!"); }
        void onDisconnect(BLEServer *server) override {
            log_i("BLE client disconnected from our server!");
        }
    };
};

#endif