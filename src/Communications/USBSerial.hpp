#pragma once

#include <cstdint>
#include <cstring>
#if SIMULATOR
typedef struct cdcacm_event_t {
    int type;
    int dtr;
    int rts;
} cdcacm_event_t;
#else
#include <esp_err.h>
#include <esp_private/usb_phy.h>
#include <tinyusb.h>
#include <tusb.h>
#include <tusb_cdc_acm.h>
#include <tusb_console.h>
#endif
#include <functional>
#include <string>
#include <utility>

#include "sindarin-debug.h"

#define CONFIG_USB_CDC_RX_BUFSIZE 8192

class USBSerial {
public:
    static auto getInstance() -> USBSerial &;
    USBSerial();
    void begin();
    void initJtag();
    void lineStateChangedCallback(int itf, cdcacm_event_t *event);
    void rxLoggingCallback(int itf, cdcacm_event_t *event);
    void rxCommandCallback(int itf, cdcacm_event_t *event);
    void txSendData(const uint8_t *data, size_t length);
    void disableLogging();
    void enableLogging();

    using SerialDataListener = std::function<void(const uint8_t *data, size_t length)>;
    void registerListener(const SerialDataListener &listener);
    void popLastListener();

private:
    uint8_t buffer_[CONFIG_USB_CDC_RX_BUFSIZE + 1]{};
    enum class BootloaderState { Idle, State1, State2, State3, State4, State5, State6, State7 };
    BootloaderState currentState_{BootloaderState::Idle};
    std::vector<SerialDataListener> listeners_;
    bool loggingEnabled_ = false;

    static USBSerial *instance; // To store the instance

    static void cdcacmLoggingCallback(int itf, cdcacm_event_t *event) {
        if (instance) {
            instance->rxLoggingCallback(itf, event);
        }
    }

    static void cdcacmCommandCallback(int itf, cdcacm_event_t *event) {
        if (instance) {
            instance->rxCommandCallback(itf, event);
        }
    }

    static void cdcacmLineStateChangedCallback(int itf, cdcacm_event_t *event) {
        if (instance) {
            instance->lineStateChangedCallback(itf, event);
        }
    }
};
