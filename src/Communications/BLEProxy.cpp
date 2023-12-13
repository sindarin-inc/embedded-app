#if USE_BLE_SERVER_PROXY

#include "BLEProxy.hpp"

#include <utility>

#include "Misc/Dispatch.hpp"
#include "Misc/Platform.hpp"
#include "Misc/StringUtil.hpp"
#include "sindarin-debug.h"

#define BLEPROXY_CHARACTERISTIC_UUID "611f68af-5ffe-4944-9b8f-c0f4f5a623ee"

std::shared_ptr<BLEProxy> BLEProxy::instance = nullptr;

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

    characteristic->setCallbacks(&characteristicCallbacks_);
    this->proxyCharacteristic_ = characteristic;
    this->proxyService_ = service;
};

void BLEProxy::registerSubscriptionStatusCallback(const SubscriptionStatusCallback &callback) {
    subscriptionStatusCallback_ = callback;
}

void BLEProxy::registerWebRequestHandler(const WebRequestHandler &handler) {
    webRequestHandler_ = handler;
}

auto BLEProxy::countChunks(const SpiramString &body) -> int {
    return (body.length() + MTU_CHUNK_SIZE - 1) / MTU_CHUNK_SIZE;
}

void BLEProxy::writeUrl(const SpiramString &url, const SpiramString &method) {
    // Chunk the URL into smaller chunks
    auto numChunks = countChunks(url);

    for (int i = 0; i < numChunks; i++) {
        SpiramString chunk = url.substr(i * MTU_CHUNK_SIZE, MTU_CHUNK_SIZE);
        SpiramString chunkedValue =
            SpiRamStringFormat("U=%d/%d:%s:%s", i + 1, numChunks, method.c_str(), url.c_str());
        log_i("GATT client writing URL chunk %d/%d: %s %s", i + 1, numChunks, method.c_str(),
              chunkedValue.c_str());
        proxyCharacteristic_->setValue(chunkedValue);
        proxyCharacteristic_->notify();
    }
}

void BLEProxy::writeHeaders(std::vector<std::pair<SpiramString, SpiramString>> &headers) {
    // Chunk the headerValue into smaller chunks
    auto numChunks = headers.size();
    auto i = 0;

    for (auto &[header, value] : headers) {
        auto headerValue = SpiRamStringFormat("%s=%s", header.c_str(), value.c_str());

        SpiramString body = SpiRamStringFormat("H=%d/%d:%s", i + 1, numChunks, headerValue.c_str());
        log_i("GATT client writing header chunk %d/%d: %s", i + 1, numChunks, body.c_str());
        proxyCharacteristic_->setValue(body);
        proxyCharacteristic_->notify();
        i++;
    }
}

// Re-sending specific chunk
void BLEProxy::writeHeader(std::vector<std::pair<SpiramString, SpiramString>> &headers,
                           int chunkNumber) {
    auto numChunks = headers.size();
    auto i = 0;

    for (auto &[header, value] : headers) {
        if ((i + 1) != chunkNumber) {
            i++;
            continue;
        }
        auto headerValue = SpiRamStringFormat("%s=%s", header.c_str(), value.c_str());

        SpiramString body = SpiRamStringFormat("H=%d/%d:%s", i + 1, numChunks, headerValue.c_str());
        log_i("GATT client writing header chunk %d/%d: %s", i + 1, numChunks, body.c_str());
        proxyCharacteristic_->setValue(body);
        proxyCharacteristic_->notify();
        i++;
    }
}

void BLEProxy::writeBody(const SpiramString &body) {
    int numChunks = this->countChunks(body);

    // Send each chunk
    for (int i = 0; i < numChunks; i++) {
        SpiramString chunk = body.substr(i * MTU_CHUNK_SIZE, MTU_CHUNK_SIZE);
        SpiramString value = SpiRamStringFormat("B=%d/%d:%s", i + 1, numChunks, chunk.c_str());
        log_i("GATT client writing body chunk %d/%d: %s", i + 1, numChunks, value.c_str());
        std::vector<uint8_t> bytes;
        for (char c : value) {
            bytes.push_back(static_cast<uint8_t>(c));
        }
        proxyCharacteristic_->setValue(bytes);
        proxyCharacteristic_->notify();
        vTaskDelay(500 / portTICK_PERIOD_MS);
    }

    log_i("GATT client writing end of request");
    proxyCharacteristic_->setValue(SpiramString("E="));
    proxyCharacteristic_->notify();
}

// Re-sending specific chunk
void BLEProxy::writeBody(const SpiramString &body, int chunkNumber) {
    int numChunks = this->countChunks(body);
    SpiramString chunk = body.substr(chunkNumber * MTU_CHUNK_SIZE, MTU_CHUNK_SIZE);
    SpiramString value =
        SpiRamStringFormat("B=%d/%d:%s", chunkNumber + 1, numChunks, chunk.c_str());
    log_i("GATT client writing body chunk %d/%d: %s", chunkNumber + 1, numChunks, value.c_str());
    proxyCharacteristic_->setValue(value);
    proxyCharacteristic_->notify();
}

#endif // USE_BLE_SERVER_PROXY
