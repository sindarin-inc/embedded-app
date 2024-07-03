#include "Communications/USBSerial.hpp"

extern USBSerial usbSerial;

auto USBSerial::getInstance() -> USBSerial & { return usbSerial; }

USBSerial::USBSerial() {
    std::memset(buffer_, 0, sizeof(buffer_));
    instance = this;
}

void USBSerial::initJtag() {
#if SIMULATOR

#else
    tusb_cdc_acm_deinit(TINYUSB_CDC_ACM_1);
    tusb_cdc_acm_deinit(TINYUSB_CDC_ACM_0);
    tinyusb_driver_uninstall();

    vTaskDelay(2000 / portTICK_PERIOD_MS);

    usb_phy_config_t phyConf;
    std::memset(&phyConf, 0, sizeof(usb_phy_config_t));
    phyConf.controller = USB_PHY_CTRL_SERIAL_JTAG;

    usb_phy_handle_t jtagPhy;
    usb_new_phy(&phyConf, &jtagPhy);
#endif
}

// This is the main callback for the USB serial port that is used for logging.
// It simply reads from the port and the writes back that data so serial monitors
// can see the data.
void USBSerial::rxLoggingCallback(int itf, cdcacm_event_t *event) {
    size_t rxSize = 0;
#if SIMULATOR
#else
    esp_err_t ret = tinyusb_cdcacm_read(static_cast<tinyusb_cdcacm_itf_t>(itf), buffer_,
                                        sizeof(buffer_) - 1, &rxSize);
    if (ret == ESP_OK) {
        buffer_[rxSize] = '\0';
        log_i("Got data (%d bytes): %s", rxSize, buffer_);
    } else {
        log_i("Read error");
    }

    tinyusb_cdcacm_write_queue(static_cast<tinyusb_cdcacm_itf_t>(itf), buffer_, rxSize);
    tinyusb_cdcacm_write_flush(static_cast<tinyusb_cdcacm_itf_t>(itf), 0);
#endif
}

void USBSerial::disableLogging() {
    if (loggingEnabled_) {
        log_i("Disabling logging");
#if !SIMULATOR
        esp_tusb_deinit_console(TINYUSB_CDC_ACM_0); // log to usb
#endif
        loggingEnabled_ = false;
    }
}

void USBSerial::enableLogging() {
    if (!loggingEnabled_) {
        log_i("Enabling logging");
#if !SIMULATOR
        esp_tusb_init_console(TINYUSB_CDC_ACM_0);
#endif
        loggingEnabled_ = true;
    }
}

// This is the main callback for the USB serial port that is used for commands.
// It reads from the port and then processes the data and sends the command to the
// DirectCommand class for processing.
void USBSerial::rxCommandCallback(int itf, cdcacm_event_t *event) {
    size_t rxSize = 0;
#if SIMULATOR

#else
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
#if SIMULATOR

#else
    size_t chunkSize = CONFIG_USB_CDC_RX_BUFSIZE;

    if (length > chunkSize) {
        for (size_t offset = 0; offset < length; offset += chunkSize) {
            size_t bytesToSend = std::min(chunkSize, length - offset);
            tinyusb_cdcacm_write_queue(TINYUSB_CDC_ACM_1, data + offset, bytesToSend);
            tinyusb_cdcacm_write_flush(TINYUSB_CDC_ACM_1, 0);
            // From experimentation, 2 MS wasn't enough and the output was getting garbled
            vTaskDelay(5 / portTICK_PERIOD_MS);
        }
    } else {
        tinyusb_cdcacm_write_queue(TINYUSB_CDC_ACM_1, data, length);
        tinyusb_cdcacm_write_flush(TINYUSB_CDC_ACM_1, 0);
    }
#endif
}

void USBSerial::begin() {
#if SIMULATOR
#else
    const tinyusb_config_t tusbCfg = {.device_descriptor = nullptr,
                                      .string_descriptor = nullptr,
                                      .external_phy = false,
                                      .configuration_descriptor = nullptr};

    ESP_ERROR_CHECK(tinyusb_driver_install(&tusbCfg));

    tinyusb_config_cdcacm_t acmCfg = {.usb_dev = TINYUSB_USBDEV_0,
                                      .cdc_port = TINYUSB_CDC_ACM_0,
                                      .callback_rx = nullptr,
                                      .callback_rx_wanted_char = nullptr,
                                      .callback_line_state_changed = nullptr,
                                      .callback_line_coding_changed = nullptr};

    ESP_ERROR_CHECK(tusb_cdc_acm_init(&acmCfg));

    enableLogging();

    vTaskDelay(2000 / portTICK_PERIOD_MS);

    log_i("Completed port CDC_ACM_0 initialization");

    tinyusb_config_cdcacm_t acmCfg1 = {.usb_dev = TINYUSB_USBDEV_0,
                                       .cdc_port = TINYUSB_CDC_ACM_1,
                                       .callback_rx = USBSerial::cdcacmCommandCallback,
                                       .callback_rx_wanted_char = nullptr,
                                       .callback_line_state_changed = nullptr,
                                       .callback_line_coding_changed = nullptr};

    ESP_ERROR_CHECK(tusb_cdc_acm_init(&acmCfg1));
    log_i("Finished port CDC_ACM_1 initialization attempt");
#endif
}