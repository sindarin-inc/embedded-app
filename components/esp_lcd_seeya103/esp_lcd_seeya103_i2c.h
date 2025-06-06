/*
 * SPDX-FileCopyrightText: 2025 Sindarin Inc.
 *
 * Driver for SeeYA SY103WAM01 (1.03" micro-OLED, 2560x2560) via I2C
 */

#pragma once

#include <driver/i2c_master.h>
#include <memory.h>

#include "esp_lcd_seeya103.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize I2C device for SeeYA display on existing bus
 *
 * @param[in] bus_handle Existing I2C bus handle
 * @param[out] dev_handle Pointer to store the I2C device handle
 * @return
 *      - ESP_OK on success
 *      - ESP_ERR_INVALID_ARG if parameter is invalid
 *      - ESP_ERR_NO_MEM if memory allocation failed
 *      - ESP_FAIL if initialization failed
 */
esp_err_t seeya103_i2c_init(i2c_master_bus_handle_t bus_handle,
                            i2c_master_dev_handle_t *dev_handle);

/**
 * @brief Deinitialize I2C device for SeeYA display
 *
 * @param[in] dev_handle I2C device handle
 * @return
 *      - ESP_OK on success
 *      - ESP_ERR_INVALID_ARG if parameter is invalid
 */
esp_err_t seeya103_i2c_deinit(i2c_master_dev_handle_t dev_handle);

/**
 * @brief Write to a register on the SeeYA display via I2C
 *
 * @param[in] dev_handle I2C device handle
 * @param[in] reg_addr Register address
 * @param[in] data Data to write
 * @return
 *      - ESP_OK on success
 *      - ESP_ERR_INVALID_ARG if parameter is invalid
 *      - ESP_FAIL if write failed
 */
esp_err_t seeya103_i2c_write_reg(i2c_master_dev_handle_t dev_handle, uint16_t reg_addr,
                                 uint16_t data);

/**
 * @brief Read from a register on the SeeYA display via I2C
 *
 * @param[in] dev_handle I2C device handle
 * @param[in] reg_addr Register address
 * @param[out] data Pointer to store read data
 * @return
 *      - ESP_OK on success
 *      - ESP_ERR_INVALID_ARG if parameter is invalid
 *      - ESP_FAIL if read failed
 */
esp_err_t seeya103_i2c_read_reg(i2c_master_dev_handle_t dev_handle, uint16_t reg_addr,
                                uint16_t *data);

/**
 * @brief Send a command to the SeeYA display via I2C
 *
 * @param[in] dev_handle I2C device handle
 * @param[in] cmd Command to send
 * @return
 *      - ESP_OK on success
 *      - ESP_ERR_INVALID_ARG if parameter is invalid
 */
esp_err_t seeya103_i2c_send_cmd(i2c_master_dev_handle_t dev_handle, seeya103_lcd_init_cmd_t cmd);

/**
 * @brief Send initialization commands to the SeeYA display via I2C
 *
 * @param[in] dev_handle I2C device handle
 * @param[in] width Display width
 * @param[in] height Display height
 * @param[in] x_offset X offset
 * @param[in] y_offset Y offset
 * @param[in] scaling Display scaling
 * @param[in] mirror_x Mirror horizontally
 * @param[in] mirror_y Mirror vertically
 * @param[in] init_cmds Pointer to initialization commands array
 * @param[in] init_cmds_size Number of commands in array
 * @return
 *      - ESP_OK on success
 *      - ESP_ERR_INVALID_ARG if parameter is invalid
 *      - ESP_FAIL if command sending failed
 */
esp_err_t seeya103_i2c_send_cmds(i2c_master_dev_handle_t dev_handle, uint16_t width,
                                 uint16_t height, uint16_t x_offset, uint16_t y_offset,
                                 seeya103_scaling_t scaling, bool mirror_x, bool mirror_y,
                                 const seeya103_lcd_init_cmd_t *init_cmds, uint16_t init_cmds_size);

/**
 * @brief Turn display on/off via I2C
 *
 * @param[in] dev_handle I2C device handle
 * @param[in] on_off true to turn on, false to turn off
 * @return
 *      - ESP_OK on success
 *      - ESP_ERR_INVALID_ARG if parameter is invalid
 *      - ESP_FAIL if command failed
 */
esp_err_t seeya103_i2c_disp_on_off(i2c_master_dev_handle_t dev_handle, bool on_off);

/**
 * @brief Move display area via I2C
 *
 * @param[in] dev_handle I2C device handle
 * @param[in] x_offset X offset
 * @param[in] y_offset Y offset
 * @return
 *      - ESP_OK on success
 *      - ESP_ERR_INVALID_ARG if parameter is invalid
 *      - ESP_FAIL if command failed
 */
esp_err_t seeya103_i2c_move_display_area(i2c_master_dev_handle_t dev_handle, int16_t x_offset,
                                         int16_t y_offset);

#ifdef __cplusplus
}
#endif