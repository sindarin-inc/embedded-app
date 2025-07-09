#if CONFIG_USE_STHS34PF80

#include "Sensors/Sths34pf80.hpp"

#include <esp_check.h>
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "sindarin-debug.h"
#include "sths34pf80.h"

constexpr const char *TAG = "STHS34PF80";

auto Sths34pf80::begin(i2c_master_bus_handle_t i2cBusHandle) -> esp_err_t {
    ESP_RETURN_ON_ERROR(
        sths34pf80_i2c_init(i2cBusHandle, STHS34PF80_I2C_ADDR_DEFAULT, &dev_handle_), TAG,
        "Failed to create STHS34PF80");

    // Reboot the sensor to ensure clean state
    ESP_RETURN_ON_ERROR(sths34pf80_reboot_and_wait(dev_handle_), TAG,
                        "Failed to reboot STHS34PF80");

    // Configure presence detection
    uint16_t presence_threshold = 1000; // Adjust based on testing
    ESP_RETURN_ON_ERROR(sths34pf80_write_embedded_register(dev_handle_, STHS34PF80_PRESENCE_THS_L,
                                                           presence_threshold & 0xFF),
                        TAG, "Failed to set presence threshold low byte");
    ESP_RETURN_ON_ERROR(sths34pf80_write_embedded_register(dev_handle_, STHS34PF80_PRESENCE_THS_H,
                                                           presence_threshold >> 8),
                        TAG, "Failed to set presence threshold high byte");

    // Reset algorithms
    ESP_RETURN_ON_ERROR(sths34pf80_reset_algorithms(dev_handle_), TAG,
                        "Failed to reset algorithms");

    // Set output data rate to 2Hz for presence detection
    ESP_RETURN_ON_ERROR(sths34pf80_write_reg(dev_handle_, STHS34PF80_CTRL1,
                                             STHS34PF80_CTRL1_BDU_MASK | STHS34PF80_ODR_2_HZ),
                        TAG, "Failed to set output data rate");

    return ESP_OK;
}

auto Sths34pf80::end() -> esp_err_t {
    if (dev_handle_) {
        // Deinitialize I2C device
        ESP_RETURN_ON_ERROR(sths34pf80_i2c_deinit(dev_handle_), TAG,
                            "Failed to deinitialize STHS34PF80");
        dev_handle_ = nullptr;
    }
    return ESP_OK;
}

auto Sths34pf80::powerDown() -> esp_err_t {
    return sths34pf80_write_reg(dev_handle_, STHS34PF80_CTRL1, STHS34PF80_ODR_POWER_DOWN);
}

auto Sths34pf80::readDeviceId(uint8_t *deviceIdOut) -> esp_err_t {
    return sths34pf80_read_reg(dev_handle_, STHS34PF80_WHO_AM_I, deviceIdOut);
}

auto Sths34pf80::configureInterrupts() -> esp_err_t {
    // Configure interrupt settings
    // INT_H_L[7] = 1 (Active low)
    // PP_OD[6] = 0 (Push-pull)
    // INT_MSK2[5] = 1 (Presence interrupt enabled)
    // INT_MSK1[4] = 0 (Motion interrupt disabled)
    // INT_MSK0[3] = 0 (Ambient temperature interrupt disabled)
    // INT_LATCHED[2] = 1 (Latched interrupt)
    // IEN[1:0] = 2 (INT_OR signal routed to INT pin)
    uint8_t ctrl3 = 0xA2;
    ESP_RETURN_ON_ERROR(sths34pf80_write_reg(dev_handle_, STHS34PF80_CTRL3, ctrl3), TAG,
                        "Failed to configure interrupt settings");

    // Clear any pending interrupts
    ESP_RETURN_ON_ERROR(clearInterruptStatus(), TAG, "Failed to clear interrupt status");

    return ESP_OK;
}

auto Sths34pf80::checkPresence(bool *presenceDetected) -> esp_err_t {
    if (!dev_handle_) {
        return ESP_ERR_INVALID_STATE;
    }

    uint8_t status;
    ESP_RETURN_ON_ERROR(sths34pf80_read_reg(dev_handle_, STHS34PF80_STATUS, &status), TAG,
                        "Failed to read status register");

    if (status & STHS34PF80_STATUS_DRDY_MASK) {
        // Read presence data
        uint8_t presence_data[2];
        ESP_RETURN_ON_ERROR(
            sths34pf80_read_bytes(dev_handle_, STHS34PF80_TPRESENCE_L, presence_data, 2), TAG,
            "Failed to read presence data");
        int16_t presence_value = (int16_t)(presence_data[1] << 8 | presence_data[0]);

        // Read function status to get presence flag
        uint8_t func_status;
        ESP_RETURN_ON_ERROR(sths34pf80_read_reg(dev_handle_, STHS34PF80_FUNC_STATUS, &func_status),
                            TAG, "Failed to read function status");

        *presenceDetected = (func_status & 0x04) ? true : false;

        log_i("Presence value: %d, flag: %d", presence_value, *presenceDetected);
    } else {
        // No data ready, assume no presence
        *presenceDetected = false;
    }

    return ESP_OK;
}

auto Sths34pf80::clearInterruptStatus() -> esp_err_t {
    if (!dev_handle_) {
        return ESP_ERR_INVALID_STATE;
    }

    // Read status registers to clear interrupts
    uint8_t status;
    ESP_RETURN_ON_ERROR(sths34pf80_read_reg(dev_handle_, STHS34PF80_STATUS, &status), TAG,
                        "Failed to read status register");

    uint8_t func_status;
    ESP_RETURN_ON_ERROR(sths34pf80_read_reg(dev_handle_, STHS34PF80_FUNC_STATUS, &func_status), TAG,
                        "Failed to read function status");

    log_i("Status: 0x%02x, Func Status: 0x%02x", status, func_status);
    return ESP_OK;
}

#endif