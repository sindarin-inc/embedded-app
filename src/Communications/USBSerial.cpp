#include "Communications/USBSerial.hpp"

#include <Spiram/Memory.hpp>
#include <esp_system.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

RTC_NOINIT_ATTR bool USBSerial::usbLoggingEnabled;
RTC_NOINIT_ATTR esp_log_level_t USBSerial::logLevel;

extern USBSerial usbSerial;

auto USBSerial::getInstance() -> USBSerial & { return usbSerial; }

void USBSerial::initJtag() {
#if CONFIG_USE_TINYUSB_SERIAL
    end();

    usb_phy_config_t phyConf;
    std::memset(&phyConf, 0, sizeof(usb_phy_config_t));
    phyConf.controller = USB_PHY_CTRL_SERIAL_JTAG;

    usb_phy_handle_t jtagPhy;
    usb_new_phy(&phyConf, &jtagPhy);
#endif
}

void USBSerial::disableLogging() {
    if (USBSerial::usbLoggingEnabled) {
        log_i("Disabling logging");
#if CONFIG_USE_TINYUSB_SERIAL
        esp_tusb_deinit_console(TINYUSB_CDC_ACM_1); // turn off log to usb
        tusb_cdc_acm_deinit(TINYUSB_CDC_ACM_1);
#endif
        USBSerial::usbLoggingEnabled = false;
    }
}

void USBSerial::enableLoggingInternal() {
#if CONFIG_USE_TINYUSB_SERIAL
    esp_tusb_init_console(TINYUSB_CDC_ACM_1);
#endif

    log_i("Logging enabled (level=%d)", esp_log_level_get("Sol"));
    USBSerial::usbLoggingEnabled = true;
}

void USBSerial::enableLogging() {
    if (!USBSerial::usbLoggingEnabled) {
        enableLoggingInternal();
    }
}

#if CONFIG_USE_TINYUSB_SERIAL
// This is the main callback for the USB serial port that is used for commands.
// It reads from the port and then processes the data and sends the command to the
// DirectCommand class for processing.
void USBSerial::rxCommandCallback(int itf, cdcacm_event_t *event) {
    size_t rxSize = 0;

    auto buffer = SpiramArrayPtr<uint8_t>(CONFIG_TINYUSB_CDC_RX_BUFSIZE + 1);

    esp_err_t ret = tinyusb_cdcacm_read(static_cast<tinyusb_cdcacm_itf_t>(itf), buffer.get(),
                                        CONFIG_TINYUSB_CDC_RX_BUFSIZE, &rxSize);
    if (ret == ESP_OK) {
        buffer[rxSize] = '\0';
        // Iterate through all the listeners registered with the USBSerial device and send them the
        // data.
        for (auto &listener : listeners_) {
            listener(buffer, rxSize);
        }
    }
}
#endif

void USBSerial::registerListener(const SerialDataListener &listener) {
    listeners_.push_back(listener);
}

void USBSerial::popLastListener() {
    if (!listeners_.empty()) {
        listeners_.pop_back();
    }
}

// For sending data back to the USB serial port. To be called by things like FileTransfer
// or DirectCommand if needed.
void USBSerial::txSendData(const uint8_t *data, size_t length) {
#if CONFIG_USE_TINYUSB_SERIAL
    size_t chunkSize = CONFIG_TINYUSB_CDC_TX_BUFSIZE;

    for (size_t offset = 0; offset < length; offset += chunkSize) {
        size_t bytesToSend = std::min(chunkSize, length - offset);
        tinyusb_cdcacm_write_queue(TINYUSB_CDC_ACM_0, data + offset, bytesToSend);
        // Block on flushing for up to 5ms to avoid losing data when flushing too quickly.
        tinyusb_cdcacm_write_flush(TINYUSB_CDC_ACM_0, pdMS_TO_TICKS(5));
    }
#endif
#if SIMULATOR
    // In the simulator, we write to the pseudo-serial port
    serialPort_->write(reinterpret_cast<const char *>(data), length);
#endif
}

void USBSerial::txSendLine(const std::string &str) {
    txSendData(reinterpret_cast<const uint8_t *>(str.c_str()), str.size());
    txSendData(reinterpret_cast<const uint8_t *>("\n"), 1);
}

void USBSerial::begin() {
    // Since we don't otherwise init this variable, we do it here on power on
    if (esp_reset_reason() == ESP_RST_POWERON) {
        USBSerial::usbLoggingEnabled = false;
        USBSerial::logLevel = ESP_LOG_INFO;
    }

#if CONFIG_USE_TINYUSB_SERIAL
    const tinyusb_config_t tusbCfg = {.device_descriptor = nullptr,
                                      .string_descriptor = nullptr,
                                      .external_phy = false,
                                      .configuration_descriptor = nullptr};

    ESP_ERROR_CHECK(tinyusb_driver_install(&tusbCfg));

    tinyusb_config_cdcacm_t acmCfg =
        {
            .usb_dev = TINYUSB_USBDEV_0,
            .cdc_port = TINYUSB_CDC_ACM_0,
            .callback_rx = USBSerial::cdcacmCommandCallback,
            .callback_rx_wanted_char = nullptr,
            .callback_line_state_changed =
                [](int itf, cdcacm_event_t *event) {
        auto *data = &event->line_state_changed_data;
        log_d("Line state changed on port 0 dtr=%d rts=%d", data->dtr, data->rts);
            },
            .callback_line_coding_changed = nullptr,
    };
    ESP_ERROR_CHECK(tusb_cdc_acm_init(&acmCfg));

    tinyusb_config_cdcacm_t acmCfg2 = {.usb_dev = TINYUSB_USBDEV_0,
                                       .cdc_port = TINYUSB_CDC_ACM_1,
                                       .callback_rx = nullptr,
                                       .callback_rx_wanted_char = nullptr,
                                       .callback_line_state_changed =
                                           [](int itf, cdcacm_event_t *event) {
        auto *data = &event->line_state_changed_data;
        log_d("Line state changed on port 1 dtr=%d rts=%d", data->dtr, data->rts);
                                       },
                                       .callback_line_coding_changed = nullptr};

    ESP_ERROR_CHECK(tusb_cdc_acm_init(&acmCfg2));
#endif

#if 0
    log_d("About to make a pseudo-serial port in the simulator");
    // In the simulator, we create a pseudo-serial port for testing
    serialPort_ = std::make_unique<AsyncPseudoSerialPort>();
    log_d("Created a pseudo-serial port and you can connect to it at %s",
          serialPort_->getSlaveName());
    serialPort_->setRxCallback([this](char *data, size_t length) {
        // Iterate through all the listeners registered with the USBSerial device and send them the
        // data.
        auto sharedData = std::shared_ptr<uint8_t[]>(new uint8_t[length]);
        std::memcpy(sharedData.get(), data, length);
        for (auto &listener : listeners_) {
            listener(sharedData, length);
        }
    });
#endif

    if (USBSerial::usbLoggingEnabled) {
        enableLoggingInternal();
    }
    setLogLevel(logLevel);
}

void USBSerial::end() {
#if CONFIG_USE_TINYUSB_SERIAL
    tusb_cdc_acm_deinit(TINYUSB_CDC_ACM_0);
    tusb_cdc_acm_deinit(TINYUSB_CDC_ACM_1);
    tinyusb_driver_uninstall();
#endif
}
