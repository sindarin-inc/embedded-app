#include "Communications/USBSerial.hpp"

extern USBSerial usbSerial;

auto USBSerial::getInstance() -> USBSerial & { return usbSerial; }

void USBSerial::initJtag() {
#if CONFIG_USE_TINYUSB_SERIAL
    tusb_cdc_acm_deinit(TINYUSB_CDC_ACM_0);
    tusb_cdc_acm_deinit(TINYUSB_CDC_ACM_1);
    tinyusb_driver_uninstall();

    usb_phy_config_t phyConf;
    std::memset(&phyConf, 0, sizeof(usb_phy_config_t));
    phyConf.controller = USB_PHY_CTRL_SERIAL_JTAG;

    usb_phy_handle_t jtagPhy;
    usb_new_phy(&phyConf, &jtagPhy);
#endif
}

void USBSerial::disableLogging() {
    if (loggingEnabled_) {
        log_i("Disabling logging");
#if CONFIG_USE_TINYUSB_SERIAL
        esp_tusb_deinit_console(TINYUSB_CDC_ACM_1); // turn off log to usb
        tusb_cdc_acm_deinit(TINYUSB_CDC_ACM_1);
#endif
        loggingEnabled_ = false;
    }
}

void USBSerial::enableLogging() {
    if (!loggingEnabled_) {
#if CONFIG_USE_TINYUSB_SERIAL
        tinyusb_config_cdcacm_t acmCfg = {.usb_dev = TINYUSB_USBDEV_0,
                                          .cdc_port = TINYUSB_CDC_ACM_1,
                                          .callback_rx = nullptr,
                                          .callback_rx_wanted_char = nullptr,
                                          .callback_line_state_changed = nullptr,
                                          .callback_line_coding_changed = nullptr};

        ESP_ERROR_CHECK(tusb_cdc_acm_init(&acmCfg));
        esp_tusb_init_console(TINYUSB_CDC_ACM_1);
#endif
        // This log message will go nowhere probably
        log_i("Logging enabled");
        loggingEnabled_ = true;
    }
}

// This is the main callback for the USB serial port that is used for commands.
// It reads from the port and then processes the data and sends the command to the
// DirectCommand class for processing.
void USBSerial::rxCommandCallback(int itf, cdcacm_event_t *event) {
#if CONFIG_USE_TINYUSB_SERIAL
    size_t rxSize = 0;
    esp_err_t ret = tinyusb_cdcacm_read(static_cast<tinyusb_cdcacm_itf_t>(itf), buffer_,
                                        sizeof(buffer_) - 1, &rxSize);
    if (ret == ESP_OK) {
        buffer_[rxSize] = '\0';
        // Iterate through all the listeners registered with the USBSerial device and send them the
        // data.
        for (auto &listener : listeners_) {
            listener(buffer_, rxSize);
        }
    }
#endif
}

void USBSerial::registerListener(const SerialDataListener &listener) {
    log_i("Registered a listener");
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
    size_t chunkSize = CONFIG_USB_CDC_RX_BUFSIZE;

    for (size_t offset = 0; offset < length; offset += chunkSize) {
        size_t bytesToSend = std::min(chunkSize, length - offset);
        tinyusb_cdcacm_write_queue(TINYUSB_CDC_ACM_0, data + offset, bytesToSend);
        // Block on flushing for up to 5ms to avoid losing data when flushing too quickly.
        tinyusb_cdcacm_write_flush(TINYUSB_CDC_ACM_0, pdMS_TO_TICKS(5));
    }
#endif
}

void USBSerial::begin() {
#if CONFIG_USE_TINYUSB_SERIAL
    const tinyusb_config_t tusbCfg = {.device_descriptor = nullptr,
                                      .string_descriptor = nullptr,
                                      .external_phy = false,
                                      .configuration_descriptor = nullptr};

    ESP_ERROR_CHECK(tinyusb_driver_install(&tusbCfg));

    tinyusb_config_cdcacm_t acmCfg = {.usb_dev = TINYUSB_USBDEV_0,
                                      .cdc_port = TINYUSB_CDC_ACM_0,
                                      .callback_rx = USBSerial::cdcacmCommandCallback,
                                      .callback_rx_wanted_char = nullptr,
                                      .callback_line_state_changed = nullptr,
                                      .callback_line_coding_changed = nullptr};

    ESP_ERROR_CHECK(tusb_cdc_acm_init(&acmCfg));
#endif
}