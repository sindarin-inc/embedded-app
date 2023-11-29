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

void BLEProxy::registerSubscriptionStatusCallback(const SubscriptionStatusCallback &callback) {
    subscriptionStatusCallback_ = callback;
}

void BLEProxy::registerWebRequestHandler(const WebRequestHandler &handler) {
    webRequestHandler_ = handler;
}

void BLEProxy::writeUrl(const SpiramString &url, const SpiramString &method) {
    // Chunk the URL into smaller chunks
    int numChunks = url.length() / MTU_CHUNK_SIZE;
    if (url.length() % MTU_CHUNK_SIZE != 0) {
        numChunks++;
    }

    for (int i = 0; i < numChunks; i++) {
        SpiramString chunk = url.substr(i * MTU_CHUNK_SIZE, MTU_CHUNK_SIZE);
        SpiramString chunkedValue =
            SpiRamStringFormat("U%d/%d:%s:%s", i + 1, numChunks, method.c_str(), url.c_str());
        log_i("GATT client writing URL chunk %d/%d: %s %s", i + 1, numChunks, method.c_str(),
              chunkedValue.c_str());
        proxyCharacteristic_->setValue(chunkedValue);
        proxyCharacteristic_->notify();
    }
}

void BLEProxy::writeHeader(const SpiramString &header, const SpiramString &value) {
    auto headerValue = SpiRamStringFormat("%s=%s", header.c_str(), value.c_str());

    // Chunk the headerValue into smaller chunks
    int numChunks = headerValue.length() / MTU_CHUNK_SIZE;
    if (headerValue.length() % MTU_CHUNK_SIZE != 0) {
        numChunks++;
    }

    for (int i = 0; i < numChunks; i++) {
        SpiramString chunk = headerValue.substr(i * MTU_CHUNK_SIZE, MTU_CHUNK_SIZE);
        SpiramString value = SpiRamStringFormat("H%d/%d:%s", i + 1, numChunks, chunk.c_str());
        log_i("GATT client writing header chunk %d/%d: %s", i + 1, numChunks, value.c_str());
        proxyCharacteristic_->setValue(value);
        proxyCharacteristic_->notify();
    }
}

void BLEProxy::writeBody(const SpiramString &body) {
    // Chunk the body into 256 byte chunks
    int numChunks = body.length() / MTU_CHUNK_SIZE;

    // If there's a remainder, add one more chunk
    if (body.length() % MTU_CHUNK_SIZE != 0) {
        numChunks++;
    }

    // Send each chunk
    for (int i = 0; i < numChunks; i++) {
        SpiramString chunk = body.substr(i * MTU_CHUNK_SIZE, MTU_CHUNK_SIZE);
        SpiramString value = SpiRamStringFormat("B%d/%d:%s", i + 1, numChunks, chunk.c_str());
        log_i("GATT client writing body chunk %d/%d: %s", i + 1, numChunks, value.c_str());
        proxyCharacteristic_->setValue(value);
        proxyCharacteristic_->notify();
    }
}

#endif // USE_BLE_SERVER_PROXY
