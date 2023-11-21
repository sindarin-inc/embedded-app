
#include "BLEProxy.hpp"

#include <driver/gpio.h>
#include <utility>

#include "Misc/Dispatch.hpp"
#include "Misc/Platform.hpp"
#include "Misc/StringUtil.hpp"
#include "Version.hpp"
#include "WiFi.hpp"
#include "sindarin-debug.h"

#define BLEPROXY_CHARACTERISTIC_UUID "611f68af-5ffe-4944-9b8f-c0f4f5a623ee"

BLEProxy::BLEProxy(PreferencesStore *prefs) { this->prefs_ = prefs; }

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

void BLEProxy::writeUrlToCharacteristic(BLECharacteristic *characteristic, const std::string &url) {
    std::string value = StringFormat("%s=%s", "url", url.c_str());
    log_i("GATT client writing url: %s", value.c_str());
    characteristic->setValue(value);
    characteristic->notify();
}

void BLEProxy::writeBodyToCharacteristic(BLECharacteristic *characteristic,
                                         const SpiramString &body) {
    std::string value = StringFormat("%s=%s", "body", body.c_str());
    log_i("GATT client writing body: %s", value.c_str());
    characteristic->setValue(value);
    characteristic->notify();
}
