#pragma once

#include <NimBLEDevice.h>
#include <memory>

#include "Communications/BLEAbstractions.hpp"
#include "sindarin-debug.h"

class BLEManager {
public:
    BLEManager();
    BLEManager(BLEManager const &) = delete;
    static auto getInstance() -> std::shared_ptr<BLEManager>;
    auto startAdvertisingIfNecessary() -> bool;
    auto stopAdvertising() -> bool;
    void enableService(BLEService *service);
    void disableService(BLEService *service);
    auto server() -> BLEServer *;

    // Extra field to advertise in the advertising packet. Mobile apps use this to identify devices
    // that belong to the logged in account.
    void setManufacturerData(const std::string &manufacturerData) {
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
        void onConnect(BLEServer *server, ble_gap_conn_desc *desc) override {
            SHOW_HEAP();
            log_i("BLE client connected to our server!");
            // Apple has _very_ specific requirements for these values and will reject these if
            // you don't give them something they want
            // https://developer.apple.com/library/archive/qa/qa1931/_index.html
            // That doc has broken links to the validation spreadsheet, but it can be found
            // here:
            // https://github.com/HoyosIntegrity/V.ALRT-bluetooth-spec/blob/master/qa1931_BLE%20Parameter%20Validation.xlsx
            // With these settings it costs <1mA to have the iOS app connected. With the default
            // settings its ~16mA
            // NOTE(john): We're rolling the back because it makes service discovery painfully slow.
            // We should experiment some more with these values after shipping -- there might be a
            // sweet spot that's faster than the defaults but still low power.
            // server->updateConnParams(desc->conn_handle, 36, 90, 2, 600);
            NimBLEDevice::setMTU(MTU_CHUNK_SIZE);
        }
        void onDisconnect(BLEServer *server) override {
            SHOW_HEAP();
            log_i("BLE client disconnected from our server!");
            enclosed->authed_ = false;
        }
    };
};
