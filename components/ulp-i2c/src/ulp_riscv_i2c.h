/*
 * SPDX-FileCopyrightText: 2022 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>
#include <hal/gpio_types.h>
#include "esp_err.h"

typedef struct {
    uint32_t sda_io_num;        // GPIO pin for SDA signal. Only GPIO#1 or GPIO#3 can be used as the SDA pin.
    uint32_t scl_io_num;        // GPIO pin for SCL signal. Only GPIO#0 or GPIO#2 can be used as the SCL pin.
    bool sda_pullup_en;         // SDA line enable internal pullup. Can be configured if external pullup is not used.
    bool scl_pullup_en;         // SCL line enable internal pullup. Can be configured if external pullup is not used.
} ulp_riscv_i2c_pin_cfg_t;

typedef struct {
    uint32_t scl_low_period;    // SCL low period
    uint32_t scl_high_period;   // SCL high period
    uint32_t sda_duty_period;   // Period between the SDA switch and the falling edge of SCL
    uint32_t scl_start_period;  // Waiting time after the START condition
    uint32_t scl_stop_period;   // Waiting time before the END condition
    uint32_t i2c_trans_timeout; // I2C transaction timeout
} ulp_riscv_i2c_timing_cfg_t;

typedef struct {
    ulp_riscv_i2c_pin_cfg_t i2c_pin_cfg;        // RTC I2C pin configuration
    ulp_riscv_i2c_timing_cfg_t i2c_timing_cfg;  // RTC I2C timing configuration
} ulp_riscv_i2c_cfg_t;

/* Nominal default GPIO settings and timing parametes */
#define ULP_RISCV_I2C_DEFAULT_CONFIG()          \
    {                                           \
        .i2c_pin_cfg.sda_io_num = GPIO_NUM_3,   \
        .i2c_pin_cfg.scl_io_num = GPIO_NUM_2,   \
        .i2c_pin_cfg.sda_pullup_en = true,      \
        .i2c_pin_cfg.scl_pullup_en = true,      \
        .i2c_timing_cfg.scl_low_period = 5,     \
        .i2c_timing_cfg.scl_high_period = 5,    \
        .i2c_timing_cfg.sda_duty_period = 2,    \
        .i2c_timing_cfg.scl_start_period = 3,   \
        .i2c_timing_cfg.scl_stop_period = 6,    \
        .i2c_timing_cfg.i2c_trans_timeout = 20, \
    }

/**
 * @brief Initialize and configure the RTC I2C for use by ULP RISC-V
 * Currently RTC I2C can only be used in master mode
 *
 * @param cfg           Configuration parameters
 * @return esp_err_t    ESP_OK when successful
 */
esp_err_t ulp_riscv_i2c_master_init(const ulp_riscv_i2c_cfg_t *cfg);

void ulp_riscv_i2c_reset();

#ifdef __cplusplus
}
#endif