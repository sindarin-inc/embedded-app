#if USE_BLE_SERVER_PROXY

#include "BLEProxy.hpp"

#include <utility>

#include "Misc/Dispatch.hpp"
#include "Misc/Platform.hpp"
#include "Misc/StringUtil.hpp"
#include "sindarin-debug.h"

#define BLEPROXY_CHARACTERISTIC_UUID "611f68af-5ffe-4944-9b8f-c0f4f5a623ee"

std::shared_ptr<BLEProxy> BLEProxy::instance = nullptr;

BLEProxy::BLEProxy() {
    proxyCharacteristic_ = nullptr;
    webRequestHandler_ = nullptr;
}

auto BLEProxy::getInstance() -> std::shared_ptr<BLEProxy> {
    if (instance == nullptr) {
        instance = std::make_shared<BLEProxy>();
    }
    return instance;
}

void BLEProxy::configureCharacteristics(BLEService *service) {
    BLECharacteristic *characteristic = service->createCharacteristic(
        BLEUUID(BLEPROXY_CHARACTERISTIC_UUID),
        NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::NOTIFY);

    characteristic->setCallbacks(new CharacteristicCallbacks(this));
    this->proxyCharacteristic_ = characteristic;
};

void BLEProxy::setWebRequestHandler(
    std::function<void(const std::string &, const SpiramString &)> handler) {
    webRequestHandler_ = std::move(handler);
}

void BLEProxy::writeUrl(const SpiramString &url) {
    std::string value = StringFormat("U:%s", url.c_str());
    log_i("GATT client writing url: %s", value.c_str());
    proxyCharacteristic_->setValue(value);
    proxyCharacteristic_->notify();
}

void BLEProxy::writeHeader(const SpiramString &header, const SpiramString &value) {
    std::string headerValue = StringFormat("H:%s=%s", header.c_str(), value.c_str());
    log_i("GATT client writing header: %s", headerValue.c_str());
    proxyCharacteristic_->setValue(headerValue);
    proxyCharacteristic_->notify();
}

void BLEProxy::writeBody(const SpiramString &body) {
    // Chunk the body into 256 byte chunks
    std::string bodyString = body.c_str();
    int chunkSize = 256;
    int numChunks = bodyString.length() / chunkSize;

    // If there's a remainder, add one more chunk
    if (bodyString.length() % chunkSize != 0) {
        numChunks++;
    }

    // Send each chunk
    for (int i = 0; i < numChunks; i++) {
        std::string chunk = bodyString.substr(i * chunkSize, chunkSize);
        std::string value = StringFormat("B%d/%d:%s", i, numChunks, chunk.c_str());
        log_i("GATT client writing body: %s", value.c_str());
        proxyCharacteristic_->setValue(value);
        proxyCharacteristic_->notify();
    }
}

#endif // USE_BLE_SERVER_PROXY
