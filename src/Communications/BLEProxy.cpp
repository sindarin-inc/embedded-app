#if CONFIG_USE_BLE_SERVER_PROXY

#include "BLEProxy.hpp"

#include <utility>

#include "Misc/Dispatch.hpp"
#include "Misc/Platform.hpp"
#include "Misc/StringUtil.hpp"
#include "sindarin-debug.h"

#define BLEPROXY_CHARACTERISTIC_UUID "611f68af-5ffe-4944-9b8f-c0f4f5a623ee"

std::shared_ptr<BLEProxy> BLEProxy::instance = nullptr;

auto BLEProxy::getInstance(bool create) -> std::shared_ptr<BLEProxy> {
    if (instance == nullptr && create) {
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

void BLEProxy::disconnect(uint16_t connId) {
    if (this->proxyService_ != nullptr) {
        if (connId != BLE_HS_CONN_HANDLE_NONE) {
            this->proxyService_->getServer()->disconnect(connId);
        }
    }
}

auto BLEProxy::countChunks(const SpiramString &body) -> int {
    return (body.length() + MTU_REDUCED_CHUNK_SIZE - 1) / MTU_REDUCED_CHUNK_SIZE;
}

void BLEProxy::writeUrl(const SpiramString &url, const SpiramString &method) {
    // Chunk the URL into smaller chunks
    auto numChunks = countChunks(url);

    for (int i = 0; i < numChunks; i++) {
        SpiramString chunk = url.substr(i * MTU_REDUCED_CHUNK_SIZE, MTU_REDUCED_CHUNK_SIZE);
        SpiramString chunkedValue =
            SpiRamStringFormat("U=%d/%d:%s:%s", i + 1, numChunks, method.c_str(), url.c_str());
        log_i("GATT client writing URL chunk %d/%d: %s %s", i + 1, numChunks, method.c_str(),
              chunkedValue.c_str());
        proxyCharacteristic_->setValue(chunkedValue);
        proxyCharacteristic_->notify();
    }
}

void BLEProxy::writeHeaders(std::vector<std::pair<SpiramString, SpiramString>> &headers,
                            int specificChunkNumber) {
    int numChunks = headers.size();
    int i = 0;

    for (auto &headerPair : headers) {
        if (specificChunkNumber < 0 || (i + 1) == specificChunkNumber) {
            sendHeaderChunk(headerPair, i, numChunks);
        }
        i++;
    }
}

void BLEProxy::sendHeaderChunk(const std::pair<SpiramString, SpiramString> &headerPair,
                               int chunkNumber, int numChunks) {
    auto [header, value] = headerPair;
    SpiramString headerValue = SpiRamStringFormat("%s=%s", header.c_str(), value.c_str());
    SpiramString body =
        SpiRamStringFormat("H=%d/%d:%s", chunkNumber + 1, numChunks, headerValue.c_str());
    log_i("GATT client writing header chunk %d/%d: %s", chunkNumber + 1, numChunks, body.c_str());
    proxyCharacteristic_->setValue(body);
    proxyCharacteristic_->notify();
}

void BLEProxy::writeBody(const SpiramString &body, int specificChunkNumber) {
    int numChunks = this->countChunks(body);

    if (specificChunkNumber < 0) {
        // Send all chunks
        for (int i = 0; i < numChunks; i++) {
            sendChunk(body, i, numChunks);

            if (i % 3 == 0) {
                vTaskDelay(250 / portTICK_PERIOD_MS);
            }
        }
        log_i("GATT client writing end of request");
        proxyCharacteristic_->setValue(SpiramString("E="));
        proxyCharacteristic_->notify();
    } else {
        // Send specific chunk
        sendChunk(body, specificChunkNumber, numChunks);
    }
}

void BLEProxy::sendChunk(const SpiramString &body, int chunkNumber, int numChunks) {
    SpiramString chunk = body.substr(chunkNumber * MTU_REDUCED_CHUNK_SIZE, MTU_REDUCED_CHUNK_SIZE);
    SpiramString checksum = calculateChecksum(chunk);
    SpiramString value = SpiRamStringFormat("B=%d/%d-%s:%s", chunkNumber + 1, numChunks,
                                            checksum.c_str(), chunk.c_str());
    if (chunkNumber == 0 || chunkNumber == numChunks - 1) {
        log_i("GATT client writing body chunk %d/%d: %zu bytes", chunkNumber + 1, numChunks,
              sizeof(chunk));
    }
    proxyCharacteristic_->setValue(value);
    proxyCharacteristic_->notify();
}

auto BLEProxy::calculateChecksum(const SpiramString &body) -> SpiramString {
    uint8_t checksum = 0;
    for (auto c : body) {
        checksum += c;
        // log_i("Checksumming %c (%d) = %d", c, c, checksum);
    }
    return SpiRamStringFormat("%02x", checksum);
}

void BLEProxy::acknowledgeKey(const SpiramString &key) {
    log_i("Acknowledging key: %s", key.c_str());
    proxyCharacteristic_->setValue(key + ".");
    proxyCharacteristic_->notify();
}

#endif // CONFIG_USE_BLE_SERVER_PROXY
