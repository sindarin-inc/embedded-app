/*
 * SPDX-FileCopyrightText: 2025 Sindarin Inc.
 *
 * Driver for SeeYA SY103WAM01 (1.03" micro-OLED, 2560x2560) via I2C
 */

#include "esp_lcd_seeya103_i2c.h"

#include <esp_check.h>
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

static const char *TAG = "seeya103";

#define I2C_MASTER_FREQ_HZ 400000  // I2C master clock frequency
#define I2C_MASTER_TIMEOUT_MS 1000 // I2C timeout in milliseconds
#define SEEYA103_I2C_ADDR 0x4C

esp_err_t seeya103_i2c_init(i2c_master_bus_handle_t bus_handle,
                            i2c_master_dev_handle_t *dev_handle) {
    ESP_RETURN_ON_FALSE(bus_handle && dev_handle, ESP_ERR_INVALID_ARG, TAG, "invalid arguments");

    i2c_device_config_t dev_config = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = SEEYA103_I2C_ADDR,
        .scl_speed_hz = I2C_MASTER_FREQ_HZ,
    };
    ESP_RETURN_ON_ERROR(i2c_master_bus_add_device(bus_handle, &dev_config, dev_handle), TAG,
                        "add I2C device failed");

    ESP_LOGD(TAG, "I2C device initialized at address 0x%02X", SEEYA103_I2C_ADDR);

    return ESP_OK;
}

esp_err_t seeya103_i2c_deinit(i2c_master_dev_handle_t dev_handle) {
    ESP_RETURN_ON_FALSE(dev_handle, ESP_ERR_INVALID_ARG, TAG, "invalid arguments");

    ESP_RETURN_ON_ERROR(i2c_master_bus_rm_device(dev_handle), TAG, "remove I2C device failed");

    return ESP_OK;
}

esp_err_t seeya103_i2c_write_reg(i2c_master_dev_handle_t dev_handle, uint16_t reg_addr,
                                 uint16_t data) {
    uint8_t write_buf[4];
    write_buf[0] = (reg_addr >> 8) & 0xFF; // Register address high byte
    write_buf[1] = reg_addr & 0xFF;        // Register address low byte
    write_buf[2] = (data >> 8) & 0xFF;     // Data high byte
    write_buf[3] = data & 0xFF;            // Data low byte

    ESP_LOGD(TAG, "I2C TX (%p) %04Xh: %04Xh", dev_handle, reg_addr, data);

    ESP_RETURN_ON_ERROR(i2c_master_transmit(dev_handle, write_buf, sizeof(write_buf),
                                            I2C_MASTER_TIMEOUT_MS / portTICK_PERIOD_MS),
                        TAG, "write register failed");

    return ESP_OK;
}

esp_err_t seeya103_i2c_read_reg(i2c_master_dev_handle_t dev_handle, uint16_t reg_addr,
                                uint16_t *data) {
    uint8_t write_buf[2];
    uint8_t read_buf[2];
    write_buf[0] = (reg_addr >> 8) & 0xFF; // Register address high byte
    write_buf[1] = reg_addr & 0xFF;        // Register address low byte

    ESP_RETURN_ON_ERROR(i2c_master_transmit_receive(dev_handle, write_buf, sizeof(write_buf),
                                                    read_buf, sizeof(read_buf),
                                                    I2C_MASTER_TIMEOUT_MS / portTICK_PERIOD_MS),
                        TAG, "read register failed");

    *data = (read_buf[0] << 8) | read_buf[1];
    return ESP_OK;
}

static esp_err_t seeya103_i2c_send_cmd(seeya103_lcd_init_cmd_t cmd, void *user_data) {
    i2c_master_dev_handle_t dev_handle = (i2c_master_dev_handle_t)user_data;

    static int data_pointer = 0;
    // MIPI commands use this special register to offset the data pointer on the following command
    if (cmd.cmd == SEEYA103_CMD_POINTER) {
        data_pointer = *(int *)cmd.data;
        return ESP_OK;
    }

    // For each command, we need to write its data bytes to sequential registers
    uint16_t reg_addr = 0;
    if (cmd.cmd > 0xFF) {
        reg_addr = cmd.cmd;
    } else {
        reg_addr = (cmd.cmd << 8);
    }
    if (data_pointer > 0) {
        reg_addr += data_pointer;
    }
    if (cmd.data_bytes > 0) {
        for (int j = 0; j < cmd.data_bytes; j++) {
            uint16_t data_byte = ((uint8_t *)cmd.data)[j];
            ESP_RETURN_ON_ERROR(seeya103_i2c_write_reg(dev_handle, reg_addr, data_byte), TAG,
                                "Failed to write command data");
            reg_addr += 1;
        }
    } else {
        ESP_RETURN_ON_ERROR(seeya103_i2c_write_reg(dev_handle, reg_addr, 0), TAG,
                            "Failed to write command");
    }
    data_pointer = 0;
    return ESP_OK;
}

esp_err_t seeya103_i2c_send_cmds(i2c_master_dev_handle_t dev_handle, uint16_t width,
                                 uint16_t height, uint16_t x_offset, uint16_t y_offset,
                                 seeya103_scaling_t scaling, bool mirror_x, bool mirror_y,
                                 const seeya103_lcd_init_cmd_t *init_cmds,
                                 uint16_t init_cmds_size) {
    ESP_RETURN_ON_FALSE(dev_handle && init_cmds, ESP_ERR_INVALID_ARG, TAG, "invalid arguments");

    return panel_seeya103_send_init_cmds_with_fn(init_cmds, init_cmds_size, width, height, x_offset,
                                                 y_offset, scaling, mirror_x, mirror_y,
                                                 seeya103_i2c_send_cmd, (void *)dev_handle);
}

esp_err_t seeya103_i2c_send_cmds_with_mirror(i2c_master_dev_handle_t dev_handle, uint16_t width,
                                             uint16_t height, uint16_t x_offset, uint16_t y_offset,
                                             seeya103_scaling_t scaling, bool mirror_x,
                                             bool mirror_y,
                                             const seeya103_lcd_init_cmd_t *init_cmds,
                                             uint16_t init_cmds_size) {
    ESP_RETURN_ON_FALSE(dev_handle && init_cmds, ESP_ERR_INVALID_ARG, TAG, "invalid arguments");

    return panel_seeya103_send_init_cmds_with_fn(init_cmds, init_cmds_size, width, height, x_offset,
                                                 y_offset, scaling, mirror_x, mirror_y,
                                                 seeya103_i2c_send_cmd, (void *)dev_handle);
}

esp_err_t seeya103_i2c_disp_on_off(i2c_master_dev_handle_t dev_handle, bool on_off) {
    ESP_RETURN_ON_FALSE(dev_handle, ESP_ERR_INVALID_ARG, TAG, "invalid arguments");

    uint16_t cmd = on_off ? SEEYA103_CMD_DISPON : SEEYA103_CMD_DISPOFF;
    ESP_RETURN_ON_ERROR(seeya103_i2c_write_reg(dev_handle, cmd << 8, 0), TAG,
                        "display on/off failed");

    return ESP_OK;
}

esp_err_t seeya103_i2c_move_display_area(i2c_master_dev_handle_t dev_handle, int16_t x_offset,
                                         int16_t y_offset) {
    ESP_RETURN_ON_FALSE(dev_handle, ESP_ERR_INVALID_ARG, TAG, "invalid arguments");

    ESP_LOGI(TAG, "panel_seeya103_move_display_area_i2c: %d %d", x_offset, y_offset);

    // Send the move display area command
    if (x_offset != -1) {
        uint8_t cmd_data[2];
        cmd_data[0] = (x_offset >> 8) & 0xFF; // High byte of dx
        cmd_data[1] = x_offset & 0xFF;        // Low byte of dx
        ESP_RETURN_ON_ERROR(
            seeya103_i2c_write_reg(dev_handle, SEEYA103_CMD_CASET << 8, cmd_data[0]), TAG,
            "Failed to send move display area command");
        ESP_RETURN_ON_ERROR(
            seeya103_i2c_write_reg(dev_handle, (SEEYA103_CMD_CASET << 8) + 1, cmd_data[1]), TAG,
            "Failed to send move display area command");
    }
    // Send the move display area command
    if (y_offset != -1) {
        uint8_t cmd_data[2];
        cmd_data[0] = (y_offset >> 8) & 0xFF; // High byte of dy
        cmd_data[1] = y_offset & 0xFF;        // Low byte of dy
        ESP_RETURN_ON_ERROR(
            seeya103_i2c_write_reg(dev_handle, SEEYA103_CMD_RASET << 8, cmd_data[0]), TAG,
            "Failed to send move display area command");
        ESP_RETURN_ON_ERROR(
            seeya103_i2c_write_reg(dev_handle, (SEEYA103_CMD_RASET << 8) + 1, cmd_data[1]), TAG,
            "Failed to send move display area command");
    }
    return ESP_OK;
}