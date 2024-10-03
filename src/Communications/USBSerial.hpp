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

#include "Misc/Dispatch.hpp"
#include "sindarin-debug.h"

#if SIMULATOR
#include "Communications/AsyncPseudoSerialPort.hpp"
#endif

class USBSerial {
public:
    static auto getInstance() -> USBSerial &;
    USBSerial() = default;
    USBSerial(const USBSerial &) = delete;
    void begin();
    void end();
    void initJtag();
#if CONFIG_USE_TINYUSB_SERIAL
    void lineStateChangedCallback(int itf, cdcacm_event_t *event);
    void rxLoggingCallback(int itf, cdcacm_event_t *event);
    void rxCommandCallback(int itf, cdcacm_event_t *event);
#endif
    void txSendData(const uint8_t *data, size_t length);
    void txSendLine(const std::string &str);
    void disableLogging();
    void enableLogging();
    auto isLoggingEnabled() -> bool { return loggingEnabled; }

    using SerialDataListener = std::function<void(std::shared_ptr<uint8_t[]>, size_t length)>;
    void registerListener(const SerialDataListener &listener);
    void popLastListener();

private:
    enum class BootloaderState { Idle, State1, State2, State3, State4, State5, State6, State7 };
    BootloaderState currentState_{BootloaderState::Idle};
    std::vector<SerialDataListener> listeners_;

#if CONFIG_USE_TINYUSB_SERIAL
    static void cdcacmCommandCallback(int itf, cdcacm_event_t *event) {
        USBSerial::getInstance().rxCommandCallback(itf, event);
    }
#endif

#if SIMULATOR
    std::unique_ptr<AsyncPseudoSerialPort> serialPort_ = nullptr;
#endif
    void enableLoggingInternal();

    static bool loggingEnabled;
};
