/*
 * SPDX-FileCopyrightText: 2025 Sindarin Inc.
 *
 * Driver for STHS34PF80TR human presence and motion detection sensor
 */

#pragma once

#include <driver/i2c_master.h>
#include <esp_err.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define STHS34PF80_I2C_ADDR_DEFAULT 0x5A
#define STHS34PF80_I2C_ADDR_ALT 0x5B

// Basic Registers
#define STHS34PF80_LPF1 0x0C
#define STHS34PF80_LPF2 0x0D
#define STHS34PF80_WHO_AM_I 0x0F
#define STHS34PF80_AVG_TRIM 0x10
#define STHS34PF80_CTRL0 0x17
#define STHS34PF80_SENS_DATA 0x1D
#define STHS34PF80_CTRL1 0x20
#define STHS34PF80_CTRL2 0x21
#define STHS34PF80_CTRL3 0x22
#define STHS34PF80_STATUS 0x23
#define STHS34PF80_FUNC_STATUS 0x25

// Temperature & Detection Data
#define STHS34PF80_TOBJECT_L 0x26
#define STHS34PF80_TOBJECT_H 0x27
#define STHS34PF80_TAMBIENT_L 0x28
#define STHS34PF80_TAMBIENT_H 0x29
#define STHS34PF80_TOBJ_COMP_L 0x38
#define STHS34PF80_TOBJ_COMP_H 0x39
#define STHS34PF80_TPRESENCE_L 0x3A
#define STHS34PF80_TPRESENCE_H 0x3B
#define STHS34PF80_TMOTION_L 0x3C
#define STHS34PF80_TMOTION_H 0x3D
#define STHS34PF80_TAMB_SHOCK_L 0x3E
#define STHS34PF80_TAMB_SHOCK_H 0x3F

// Embedded Function Access
#define STHS34PF80_FUNC_CFG_ADDR 0x08
#define STHS34PF80_FUNC_CFG_DATA 0x09
#define STHS34PF80_PAGE_RW 0x11

// Embedded Function Registers (use FUNC_CFG_ADDR/DATA)
#define STHS34PF80_PRESENCE_THS_L 0x20
#define STHS34PF80_PRESENCE_THS_H 0x21
#define STHS34PF80_MOTION_THS_L 0x22
#define STHS34PF80_MOTION_THS_H 0x23
#define STHS34PF80_TAMB_SHOCK_THS_L 0x24
#define STHS34PF80_TAMB_SHOCK_THS_H 0x25
#define STHS34PF80_HYST_MOTION 0x26
#define STHS34PF80_HYST_PRESENCE 0x27
#define STHS34PF80_ALGO_CONFIG 0x28
#define STHS34PF80_HYST_TAMB_SHOCK 0x29
#define STHS34PF80_RESET_ALGO 0x2A

#define STHS34PF80_WHOAMI_VALUE 0xD3

#define STHS34PF80_CTRL1_ODR_MASK 0x0F
#define STHS34PF80_CTRL1_BDU_MASK 0x10
#define STHS34PF80_CTRL2_ONESHOT_MASK 0x01

#define STHS34PF80_STATUS_DRDY_MASK 0x04

typedef enum {
    STHS34PF80_ODR_POWER_DOWN = 0x00,
    STHS34PF80_ODR_0_25_HZ = 0x01,
    STHS34PF80_ODR_0_5_HZ = 0x02,
    STHS34PF80_ODR_1_HZ = 0x03,
    STHS34PF80_ODR_2_HZ = 0x04,
    STHS34PF80_ODR_4_HZ = 0x05,
    STHS34PF80_ODR_8_HZ = 0x06,
    STHS34PF80_ODR_15_HZ = 0x07,
    STHS34PF80_ODR_30_HZ = 0x08
} sths34pf80_odr_t;

/**
 * @brief Initialize I2C device for STHS34PF80 sensor on existing bus
 *
 * @param[in] bus_handle Existing I2C bus handle
 * @param[in] device_addr I2C device address (use STHS34PF80_I2C_ADDR_DEFAULT or
 * STHS34PF80_I2C_ADDR_ALT)
 * @param[out] dev_handle Pointer to store the I2C device handle
 * @return
 *      - ESP_OK on success
 *      - ESP_ERR_INVALID_ARG if parameter is invalid
 *      - ESP_ERR_NO_MEM if memory allocation failed
 *      - ESP_FAIL if initialization failed
 */
esp_err_t sths34pf80_i2c_init(i2c_master_bus_handle_t bus_handle, uint8_t device_addr,
                              i2c_master_dev_handle_t *dev_handle);

/**
 * @brief Deinitialize I2C device for STHS34PF80 sensor
 *
 * @param[in] dev_handle I2C device handle
 * @return
 *      - ESP_OK on success
 *      - ESP_ERR_INVALID_ARG if parameter is invalid
 */
esp_err_t sths34pf80_i2c_deinit(i2c_master_dev_handle_t dev_handle);

/**
 * @brief Write to a register on the STHS34PF80 sensor
 *
 * @param[in] dev_handle I2C device handle
 * @param[in] reg_addr Register address (8-bit)
 * @param[in] data Data to write (8-bit)
 * @return
 *      - ESP_OK on success
 *      - ESP_ERR_INVALID_ARG if parameter is invalid
 *      - ESP_FAIL if write failed
 */
esp_err_t sths34pf80_write_reg(i2c_master_dev_handle_t dev_handle, uint8_t reg_addr, uint8_t data);

/**
 * @brief Read from a register on the STHS34PF80 sensor
 *
 * @param[in] dev_handle I2C device handle
 * @param[in] reg_addr Register address (8-bit)
 * @param[out] data Pointer to store read data (8-bit)
 * @return
 *      - ESP_OK on success
 *      - ESP_ERR_INVALID_ARG if parameter is invalid
 *      - ESP_FAIL if read failed
 */
esp_err_t sths34pf80_read_reg(i2c_master_dev_handle_t dev_handle, uint8_t reg_addr, uint8_t *data);

/**
 * @brief Write multiple bytes to consecutive registers (auto-increment)
 *
 * @param[in] dev_handle I2C device handle
 * @param[in] reg_addr Starting register address (8-bit)
 * @param[in] data Pointer to data buffer
 * @param[in] len Number of bytes to write
 * @return
 *      - ESP_OK on success
 *      - ESP_ERR_INVALID_ARG if parameter is invalid
 *      - ESP_FAIL if write failed
 */
esp_err_t sths34pf80_write_bytes(i2c_master_dev_handle_t dev_handle, uint8_t reg_addr,
                                 const uint8_t *data, size_t len);

/**
 * @brief Read multiple bytes from consecutive registers (auto-increment)
 *
 * @param[in] dev_handle I2C device handle
 * @param[in] reg_addr Starting register address (8-bit)
 * @param[out] data Pointer to data buffer
 * @param[in] len Number of bytes to read
 * @return
 *      - ESP_OK on success
 *      - ESP_ERR_INVALID_ARG if parameter is invalid
 *      - ESP_FAIL if read failed
 */
esp_err_t sths34pf80_read_bytes(i2c_master_dev_handle_t dev_handle, uint8_t reg_addr, uint8_t *data,
                                size_t len);

/**
 * @brief Write to an embedded register on the STHS34PF80 sensor
 *
 * @param[in] dev_handle I2C device handle
 * @param[in] reg Register address (8-bit)
 * @param[in] value Data to write (8-bit)
 * @return
 *      - ESP_OK on success
 *      - ESP_ERR_INVALID_ARG if parameter is invalid
 *      - ESP_FAIL if write failed
 */
esp_err_t sths34pf80_write_embedded_register(i2c_master_dev_handle_t dev_handle, uint8_t reg,
                                             uint8_t value);

/**
 * @brief Read from an embedded register on the STHS34PF80 sensor
 *
 * @param[in] dev_handle I2C device handle
 * @param[in] reg Register address (8-bit)
 * @param[out] value Pointer to store read data (8-bit)
 * @return
 *      - ESP_OK on success
 *      - ESP_ERR_INVALID_ARG if parameter is invalid
 *      - ESP_FAIL if read failed
 */
esp_err_t sths34pf80_read_embedded_register(i2c_master_dev_handle_t dev_handle, uint8_t reg,
                                            uint8_t *value);

/**
 * @brief Reset the algorithms on the STHS34PF80 sensor
 *
 * @param[in] dev_handle I2C device handle
 * @return
 *      - ESP_OK on success
 *      - ESP_ERR_INVALID_ARG if parameter is invalid
 *      - ESP_FAIL if reset failed
 */
esp_err_t sths34pf80_reset_algorithms(i2c_master_dev_handle_t dev_handle);

/**
 * @brief Reboot the STHS34PF80 sensor and wait for it to be ready
 *
 * @param[in] dev_handle I2C device handle
 * @return
 *      - ESP_OK on success
 *      - ESP_ERR_INVALID_ARG if parameter is invalid
 *      - ESP_FAIL if reboot failed
 */
esp_err_t sths34pf80_reboot_and_wait(i2c_master_dev_handle_t dev_handle);

#ifdef __cplusplus
}
#endif