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

    // Extra field to advertise in the advertising packet. Mobile apps use this to identify devices
    // that belong to the logged in account.
    void setManfacturerData(const std::string &manufacturerData) {
        server()->getAdvertising()->setManufacturerData(manufacturerData);
    }

    void markAsAuthed() { authed_ = true; }
    auto isAuthed() -> bool { return authed_; }
    auto checkAuthed(ble_gap_conn_desc *desc) -> bool {
        if (!authed_) {
            log_w("Client not authenticated, disconnecting");
            server()->disconnect(desc->conn_handle);
            return false;
        }
        return true;
    }

protected:
    static std::shared_ptr<BLEManager> instance;
    BLEServer *server_ = nullptr;
    bool authed_ = false;

    class ServerCallbacks : public BLEServerCallbacks {
    public:
        BLEManager *enclosed;
        ServerCallbacks(BLEManager *enclosure) { enclosed = enclosure; }
        void onConnect(BLEServer *server) override {
            SHOW_HEAP();
            log_i("BLE client connected to our server!");
        }
        void onDisconnect(BLEServer *server) override {
            SHOW_HEAP();
            log_i("BLE client disconnected from our server!");
            enclosed->authed_ = false;
        }
    };
};

#endif
