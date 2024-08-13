#pragma once

#include <cstdint>
#include <cstring>
#include <esp_err.h>
#include <memory>
#include <tinyusb.h>

#if CONFIG_USE_TINYUSB_SERIAL
#include <esp_private/usb_phy.h>
#include <tusb.h>
#include <tusb_cdc_acm.h>
#include <tusb_console.h>
#endif
#include <functional>
#include <string>
#include <utility>

#include "sindarin-debug.h"

class USBSerial {
public:
    static auto getInstance() -> USBSerial &;
    USBSerial() = default;
    USBSerial(const USBSerial &) = delete;
    void begin();
    void initJtag();
    void lineStateChangedCallback(int itf, cdcacm_event_t *event);
    void rxLoggingCallback(int itf, cdcacm_event_t *event);
    void rxCommandCallback(int itf, cdcacm_event_t *event);
    void txSendData(const uint8_t *data, size_t length);
    void disableLogging();
    void enableLogging();

    using SerialDataListener = std::function<void(std::shared_ptr<uint8_t[]>, size_t length)>;
    void registerListener(const SerialDataListener &listener);
    void popLastListener();

private:
    enum class BootloaderState { Idle, State1, State2, State3, State4, State5, State6, State7 };
    BootloaderState currentState_{BootloaderState::Idle};
    std::vector<SerialDataListener> listeners_;
    bool loggingEnabled_ = false;

    static void cdcacmCommandCallback(int itf, cdcacm_event_t *event) {
        USBSerial::getInstance().rxCommandCallback(itf, event);
    }
};
