#pragma once

#include <cstdint>
#include <cstring>
#include <esp_err.h>
#include <esp_private/usb_phy.h>
#include <functional>
#include <string>
#include <tinyusb.h>
#include <tusb.h>
#include <tusb_cdc_acm.h>
#include <tusb_console.h>
#include <utility>

#include "sindarin-debug.h"

#define CONFIG_USB_CDC_RX_BUFSIZE 8192

class USBSerial {
public:
    static auto getInstance() -> USBSerial &;
    USBSerial();
    void begin();
    void InitJtag();
    void LineStateChangedCallback(int itf, cdcacm_event_t *event);
    void RxLoggingCallback(int itf, cdcacm_event_t *event);
    void RxCommandCallback(int itf, cdcacm_event_t *event);
    void TxSendData(const uint8_t *data, size_t length);
    void DisableLogging();
    void EnableLogging();

    using SerialDataListener = std::function<void(const uint8_t *data, size_t length)>;
    void RegisterListener(SerialDataListener listener);
    void PopLastListener();

private:
    uint8_t buffer_[CONFIG_USB_CDC_RX_BUFSIZE + 1];
    enum class BootloaderState { Idle, State1, State2, State3, State4, State5, State6, State7 };
    BootloaderState currentState_;
    std::vector<SerialDataListener> listeners_;
    bool loggingEnabled_ = false;

    static USBSerial *instance; // To store the instance

    static void cdcacmLoggingCallback(int itf, cdcacm_event_t *event) {
        if (instance) {
            instance->RxLoggingCallback(itf, event);
        }
    }

    static void cdcacmCommandCallback(int itf, cdcacm_event_t *event) {
        if (instance) {
            instance->RxCommandCallback(itf, event);
        }
    }

    static void cdcacmLineStateChangedCallback(int itf, cdcacm_event_t *event) {
        if (instance) {
            instance->LineStateChangedCallback(itf, event);
        }
    }
};
