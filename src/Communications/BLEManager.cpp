#if USE_BLE_SERVER_REMOTE

#include "BLEManager.hpp"

#include "config.h"

std::shared_ptr<BLEManager> BLEManager::instance = nullptr;

auto BLEManager::getInstance() -> std::shared_ptr<BLEManager> {
    if (instance == nullptr) {
        instance = std::make_shared<BLEManager>();
    }
    return instance;
}

BLEManager::BLEManager() = default;

auto BLEManager::server() -> BLEServer * {
    if (server_ == nullptr) {
        server_ = BLEDevice::createServer();
        server_->setCallbacks(new ServerCallbacks());
    }
    return server_;
}

auto BLEManager::startAdvertisingIfNecessary() -> bool {
    if (server_ != nullptr) {
        auto advertising = server_->getAdvertising();

        // TODO: When is Max used vs Min?
        // See also TurnTouch defaults
        // https://github.com/samuelclay/turntouch-remote/blob/f21e795a40b84af7779d7cf2d032c4d81fedc363/firmware/src/main.h#L39
        // Also see also Apples accessory guide
        // https://developer.apple.com/accessories/Accessory-Design-Guidelines.pdf
        // The TurnTouch uses 3200 which is ~2s. Our app can discover that just fine but won't
        // connect. It's unclear if that's a problem from the ESP/firmware or with our app or both.
        // 510 (318.75ms) was the max that worked reliably from my experimentation
        // This still allows for ~90% (vs 80% with defaults) light sleep time when advertising, so
        // it's probably fine.
        advertising->setMaxInterval(510);
        // Min advertising interval (in units of 0.625 ms). The default value corresponds to 20 ms.
        advertising->setMinInterval(32);
        // TODO: Investigate why power is so much higher when the iOS app is connected.
        return advertising->start();
    }
    return false;
}

#endif
