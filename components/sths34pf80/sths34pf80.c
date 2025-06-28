/*
 * SPDX-FileCopyrightText: 2025 Sindarin Inc.
 *
 * Driver for STHS34PF80TR human presence and motion detection sensor
 */

#include "sths34pf80.h"

#include <esp_check.h>
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <stdlib.h>
#include <string.h>

static const char *TAG = "sths34pf80";

#define I2C_MASTER_FREQ_HZ 400000
#define I2C_MASTER_TIMEOUT_MS 1000

esp_err_t sths34pf80_i2c_init(i2c_master_bus_handle_t bus_handle, uint8_t device_addr,
                              i2c_master_dev_handle_t *dev_handle) {
    ESP_RETURN_ON_FALSE(bus_handle && dev_handle, ESP_ERR_INVALID_ARG, TAG, "invalid arguments");

    i2c_device_config_t dev_config = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = device_addr,
        .scl_speed_hz = I2C_MASTER_FREQ_HZ,
    };
    ESP_RETURN_ON_ERROR(i2c_master_bus_add_device(bus_handle, &dev_config, dev_handle), TAG,
                        "add I2C device failed");

    ESP_LOGD(TAG, "I2C device initialized at address 0x%02X", device_addr);

    return ESP_OK;
}

esp_err_t sths34pf80_i2c_deinit(i2c_master_dev_handle_t dev_handle) {
    ESP_RETURN_ON_FALSE(dev_handle, ESP_ERR_INVALID_ARG, TAG, "invalid arguments");

    ESP_RETURN_ON_ERROR(i2c_master_bus_rm_device(dev_handle), TAG, "remove I2C device failed");

    return ESP_OK;
}

esp_err_t sths34pf80_write_bytes(i2c_master_dev_handle_t dev_handle, uint8_t reg_addr,
                                 const uint8_t *data, size_t len) {
    ESP_RETURN_ON_FALSE(dev_handle && data && len > 0, ESP_ERR_INVALID_ARG, TAG,
                        "invalid arguments");

    uint8_t *write_buf = malloc(len + 1);
    ESP_RETURN_ON_FALSE(write_buf, ESP_ERR_NO_MEM, TAG, "memory allocation failed");

    write_buf[0] = reg_addr;
    memcpy(write_buf + 1, data, len);

    ESP_LOGD(TAG, "I2C write %zu bytes starting at reg 0x%02X", len, reg_addr);

    esp_err_t ret = i2c_master_transmit(dev_handle, write_buf, len + 1,
                                        I2C_MASTER_TIMEOUT_MS / portTICK_PERIOD_MS);

    free(write_buf);

    ESP_RETURN_ON_ERROR(ret, TAG, "write bytes failed");

    return ESP_OK;
}

esp_err_t sths34pf80_read_bytes(i2c_master_dev_handle_t dev_handle, uint8_t reg_addr, uint8_t *data,
                                size_t len) {
    ESP_RETURN_ON_FALSE(dev_handle && data && len > 0, ESP_ERR_INVALID_ARG, TAG,
                        "invalid arguments");

    ESP_RETURN_ON_ERROR(i2c_master_transmit_receive(dev_handle, &reg_addr, 1, data, len,
                                                    I2C_MASTER_TIMEOUT_MS / portTICK_PERIOD_MS),
                        TAG, "read bytes failed");

    ESP_LOGD(TAG, "I2C read %zu bytes starting at reg 0x%02X", len, reg_addr);

    return ESP_OK;
}

esp_err_t sths34pf80_write_reg(i2c_master_dev_handle_t dev_handle, uint8_t reg_addr, uint8_t data) {
    return sths34pf80_write_bytes(dev_handle, reg_addr, &data, 1);
}

esp_err_t sths34pf80_read_reg(i2c_master_dev_handle_t dev_handle, uint8_t reg_addr, uint8_t *data) {
    return sths34pf80_read_bytes(dev_handle, reg_addr, data, 1);
}

esp_err_t sths34pf80_write_embedded_register(i2c_master_dev_handle_t dev_handle, uint8_t reg,
                                             uint8_t value) {
    ESP_RETURN_ON_ERROR(sths34pf80_write_reg(dev_handle, STHS34PF80_CTRL2, 0x10), TAG,
                        "write CTRL2 failed"); // FUNC_CFG_ACCESS = 1
    ESP_RETURN_ON_ERROR(sths34pf80_write_reg(dev_handle, STHS34PF80_PAGE_RW, 0x40), TAG,
                        "write PAGE_RW failed"); // FUNC_CFG_WRITE = 1
    ESP_RETURN_ON_ERROR(sths34pf80_write_reg(dev_handle, STHS34PF80_FUNC_CFG_ADDR, reg), TAG,
                        "write FUNC_CFG_ADDR failed");
    ESP_RETURN_ON_ERROR(sths34pf80_write_reg(dev_handle, STHS34PF80_FUNC_CFG_DATA, value), TAG,
                        "write FUNC_CFG_DATA failed");
    ESP_RETURN_ON_ERROR(sths34pf80_write_reg(dev_handle, STHS34PF80_PAGE_RW, 0x00), TAG,
                        "write PAGE_RW failed");
    ESP_RETURN_ON_ERROR(sths34pf80_write_reg(dev_handle, STHS34PF80_CTRL2, 0x00), TAG,
                        "write CTRL2 failed");
    return ESP_OK;
}

esp_err_t sths34pf80_read_embedded_register(i2c_master_dev_handle_t dev_handle, uint8_t reg,
                                            uint8_t *value) {
    ESP_RETURN_ON_ERROR(sths34pf80_write_reg(dev_handle, STHS34PF80_CTRL2, 0x10), TAG,
                        "write CTRL2 failed"); // FUNC_CFG_ACCESS = 1
    ESP_RETURN_ON_ERROR(sths34pf80_write_reg(dev_handle, STHS34PF80_PAGE_RW, 0x20), TAG,
                        "write PAGE_RW failed"); // FUNC_CFG_READ = 1
    ESP_RETURN_ON_ERROR(sths34pf80_write_reg(dev_handle, STHS34PF80_FUNC_CFG_ADDR, reg), TAG,
                        "write FUNC_CFG_ADDR failed");
    ESP_RETURN_ON_ERROR(sths34pf80_read_reg(dev_handle, STHS34PF80_FUNC_CFG_DATA, value), TAG,
                        "read FUNC_CFG_DATA failed");
    ESP_RETURN_ON_ERROR(sths34pf80_write_reg(dev_handle, STHS34PF80_PAGE_RW, 0x00), TAG,
                        "write PAGE_RW failed");
    ESP_RETURN_ON_ERROR(sths34pf80_write_reg(dev_handle, STHS34PF80_CTRL2, 0x00), TAG,
                        "write CTRL2 failed");
    return ESP_OK;
}

esp_err_t sths34pf80_reset_algorithms(i2c_master_dev_handle_t dev_handle) {
    return sths34pf80_write_embedded_register(dev_handle, STHS34PF80_RESET_ALGO, 0x01);
}

esp_err_t sths34pf80_reboot_and_wait(i2c_master_dev_handle_t dev_handle) {
    ESP_RETURN_ON_ERROR(sths34pf80_write_reg(dev_handle, STHS34PF80_CTRL2, 0x80), TAG,
                        "write CTRL2 failed");
    while (1) {
        uint8_t status;
        ESP_RETURN_ON_ERROR(sths34pf80_read_reg(dev_handle, STHS34PF80_CTRL2, &status), TAG,
                            "read CTRL2 failed");
        if ((status & 0x80) == 0) {
            break;
        }
        vTaskDelay(pdMS_TO_TICKS(1));
    }
    return ESP_OK;
}
