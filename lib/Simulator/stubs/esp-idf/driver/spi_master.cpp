#include "spi_master.h"

#include "spi_mock.hpp"

esp_err_t spi_bus_add_device(spi_host_device_t host_id,
                             const spi_device_interface_config_t *dev_config,
                             spi_device_handle_t *handle) {
    *handle = reinterpret_cast<spi_device_t *>(new spi_host_device_t(host_id));
    return ESP_OK;
}

esp_err_t spi_device_polling_transmit(spi_device_handle_t handle, spi_transaction_t *trans_desc) {
    spi_host_device_t host_id = *reinterpret_cast<spi_host_device_t *>(handle);
    auto callback = pollingTransmitCallbacks[host_id];
    if (callback) {
        return callback(handle, trans_desc);
    } else {
        return ESP_OK;
    }
}

esp_err_t spi_device_acquire_bus(spi_device_handle_t device, TickType_t wait) { return ESP_OK; }

void spi_device_release_bus(spi_device_handle_t dev) {}
