/*
 * SPDX-FileCopyrightText: 2022-2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "ulp_riscv_i2c.h"
#include <esp_check.h>
#include <soc/rtc_i2c_reg.h>
#include "rtc_i2c_struct.h"
#include <soc/rtc_io_struct.h>
#include <soc/sens_reg.h>
#include <soc/clk_tree_defs.h>
//#include <hal/i2c_ll.h>
#include <driver/rtc_io.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

static const char *RTCI2C_TAG = "ulp_riscv_i2c";

// address of RTC_I2C control register bank
// from espressif docs:
// The addresses in this section are relative to low-power management base address + 0x0C00 provided in Table
// 4-3 in Chapter 4 System and Memory.
// Low-Power Management 0x6000_8000 0x6000_8FFF 4
#define RTC_I2C 0x60008C00

/* Use the register structure to access RTC_I2C and RTCIO module registers */
rtc_i2c_dev_t *i2c_dev = (rtc_i2c_dev_t*)RTC_I2C;
rtc_io_dev_t *rtc_io_dev = &RTCIO;

#define MICROSEC_TO_RTC_FAST_CLK(period)    (period) * ((SOC_CLK_RC_FAST_FREQ_APPROX) / (1000000))

static esp_err_t i2c_gpio_is_cfg_valid(gpio_num_t sda_io_num, gpio_num_t scl_io_num)
{
    /* Verify that the SDA and SCL GPIOs are valid RTC I2C io pins */
    ESP_RETURN_ON_ERROR(!rtc_gpio_is_valid_gpio(sda_io_num), RTCI2C_TAG, "RTC I2C SDA GPIO invalid");
    ESP_RETURN_ON_ERROR(!rtc_gpio_is_valid_gpio(scl_io_num), RTCI2C_TAG, "RTC I2C SCL GPIO invalid");

    /* Verify that the SDA and SCL line belong to the RTC IO I2C function group */
    if ((sda_io_num != GPIO_NUM_1) && (sda_io_num != GPIO_NUM_3)) {
        ESP_LOGE(RTCI2C_TAG, "SDA pin can only be configured as GPIO#1 or GPIO#3");
        return ESP_ERR_INVALID_ARG;
    }

    if ((scl_io_num != GPIO_NUM_0) && (scl_io_num != GPIO_NUM_2)) {
        ESP_LOGE(RTCI2C_TAG, "SCL pin can only be configured as GPIO#0 or GPIO#2");
        return ESP_ERR_INVALID_ARG;
    }

    return ESP_OK;
}

static esp_err_t i2c_configure_io(gpio_num_t io_num, bool pullup_en)
{
    /* Initialize IO Pin */
    ESP_RETURN_ON_ERROR(rtc_gpio_init(io_num), RTCI2C_TAG, "RTC GPIO Init failed for GPIO %d", io_num);
    /* Set direction to input+output */
    ESP_RETURN_ON_ERROR(rtc_gpio_set_direction(io_num, RTC_GPIO_MODE_INPUT_OUTPUT), RTCI2C_TAG, "RTC GPIO Set direction failed for %d", io_num);
    /* Disable pulldown on the io pin */
    ESP_RETURN_ON_ERROR(rtc_gpio_pulldown_dis(io_num), RTCI2C_TAG, "RTC GPIO pulldown disable failed for %d", io_num);
    /* Enable pullup based on pullup_en flag */
    if (pullup_en) {
        ESP_RETURN_ON_ERROR(rtc_gpio_pullup_en(io_num), RTCI2C_TAG, "RTC GPIO pullup enable failed for %d", io_num);
    } else {
        ESP_RETURN_ON_ERROR(rtc_gpio_pullup_dis(io_num), RTCI2C_TAG, "RTC GPIO pullup disable failed for %d", io_num);
    }

    return ESP_OK;
}


static esp_err_t i2c_set_pin(const ulp_riscv_i2c_cfg_t *cfg)
{
    gpio_num_t sda_io_num = (gpio_num_t)cfg->i2c_pin_cfg.sda_io_num;
    gpio_num_t scl_io_num = (gpio_num_t)cfg->i2c_pin_cfg.scl_io_num;
    bool sda_pullup_en = cfg->i2c_pin_cfg.sda_pullup_en;
    bool scl_pullup_en = cfg->i2c_pin_cfg.scl_pullup_en;

    /* Verify that the I2C GPIOs are valid */
    ESP_RETURN_ON_ERROR(i2c_gpio_is_cfg_valid(sda_io_num, scl_io_num), RTCI2C_TAG, "RTC I2C GPIO config invalid");

    /* Initialize SDA Pin */
    ESP_RETURN_ON_ERROR(i2c_configure_io(sda_io_num, sda_pullup_en), RTCI2C_TAG, "RTC I2C SDA pin config failed");

    /* Initialize SCL Pin */
    ESP_RETURN_ON_ERROR(i2c_configure_io(scl_io_num, scl_pullup_en), RTCI2C_TAG, "RTC I2C SCL pin config failed");

    /* Route SDA IO signal to the RTC subsystem */
    rtc_io_dev->touch_pad[sda_io_num].mux_sel = 1;

    /* Route SCL IO signal to the RTC subsystem */
    rtc_io_dev->touch_pad[scl_io_num].mux_sel = 1;

    /* Select RTC I2C function for SDA pin */
    rtc_io_dev->touch_pad[sda_io_num].fun_sel = 3;

    /* Select RTC I2C function for SCL pin */
    rtc_io_dev->touch_pad[scl_io_num].fun_sel = 3;

    /* Map the SDA and SCL signals to the RTC I2C controller */
    if (sda_io_num == GPIO_NUM_1) {
        rtc_io_dev->sar_i2c_io.sda_sel = 0;
    } else {
        rtc_io_dev->sar_i2c_io.sda_sel = 1;
    }

    if (scl_io_num == GPIO_NUM_0) {
        rtc_io_dev->sar_i2c_io.scl_sel = 0;
    } else {
        rtc_io_dev->sar_i2c_io.scl_sel = 1;
    }

    return ESP_OK;
}

static esp_err_t i2c_set_timing(const ulp_riscv_i2c_cfg_t *cfg)
{
    /* Convert all timing parameters from micro-seconds to period in RTC_FAST_CLK cycles.
     * RTC_FAST_CLK = 8.5 MHz for esp32s2 and 17.5 MHz for esp32s3.
     * The following calculations approximate the period for each parameter.
     */
    uint32_t scl_low_period = MICROSEC_TO_RTC_FAST_CLK(cfg->i2c_timing_cfg.scl_low_period);
    uint32_t scl_high_period = MICROSEC_TO_RTC_FAST_CLK(cfg->i2c_timing_cfg.scl_high_period);
    uint32_t sda_duty_period = MICROSEC_TO_RTC_FAST_CLK(cfg->i2c_timing_cfg.sda_duty_period);
    uint32_t scl_start_period = MICROSEC_TO_RTC_FAST_CLK(cfg->i2c_timing_cfg.scl_start_period);
    uint32_t scl_stop_period = MICROSEC_TO_RTC_FAST_CLK(cfg->i2c_timing_cfg.scl_stop_period);
    uint32_t i2c_trans_timeout = MICROSEC_TO_RTC_FAST_CLK(cfg->i2c_timing_cfg.i2c_trans_timeout);
    uint32_t setup_time_start = (cfg->i2c_timing_cfg.scl_high_period + cfg->i2c_timing_cfg.sda_duty_period);
    uint32_t hold_time_start = (cfg->i2c_timing_cfg.scl_start_period - cfg->i2c_timing_cfg.sda_duty_period);
    uint32_t setup_time_data = (cfg->i2c_timing_cfg.scl_low_period - cfg->i2c_timing_cfg.sda_duty_period);

    /* Verify timing constraints */
    ESP_RETURN_ON_FALSE((float)cfg->i2c_timing_cfg.scl_low_period > 1.3, ESP_ERR_INVALID_ARG, RTCI2C_TAG, "SCL low period cannot be less than 1.3 micro seconds");
    ESP_RETURN_ON_FALSE((float)cfg->i2c_timing_cfg.scl_high_period > 0.6, ESP_ERR_INVALID_ARG, RTCI2C_TAG, "SCL high period cannot be less than 0.6 micro seconds");
    ESP_RETURN_ON_FALSE((float)setup_time_start > 0.6, ESP_ERR_INVALID_ARG, RTCI2C_TAG, "Setup time cannot be less than 0.6 micro seconds");
    ESP_RETURN_ON_FALSE((float)hold_time_start > 0.6, ESP_ERR_INVALID_ARG, RTCI2C_TAG, "Data hold time cannot be less than 0.6 micro seconds");
    ESP_RETURN_ON_FALSE((float)cfg->i2c_timing_cfg.scl_stop_period > 0.6, ESP_ERR_INVALID_ARG, RTCI2C_TAG, "Setup time cannot be less than 0.6 micro seconds");
    ESP_RETURN_ON_FALSE((float)cfg->i2c_timing_cfg.sda_duty_period < 3.45, ESP_ERR_INVALID_ARG, RTCI2C_TAG, "Data hold time cannot be greater than 3.45 micro seconds");
    ESP_RETURN_ON_FALSE((float)(setup_time_data * 1000) > 250, ESP_ERR_INVALID_ARG, RTCI2C_TAG, "Data setup time cannot be less than 250 nano seconds");

    /* Verify filtering constrains
     *
     * I2C may have glitches on the transition edge, so the edge will be filtered in the design,
     * which will also affect the value of the timing parameter register.
     * Therefore, the following filtering constraints must be followed:
     */
    ESP_RETURN_ON_FALSE(scl_stop_period > scl_high_period, ESP_ERR_INVALID_ARG, RTCI2C_TAG, "SCL Stop period cannot be greater than SCL high period");
    ESP_RETURN_ON_FALSE(sda_duty_period < scl_low_period, ESP_ERR_INVALID_ARG, RTCI2C_TAG, "SDA duty period cannot be less than the SCL low period");
    ESP_RETURN_ON_FALSE(scl_start_period > 8, ESP_ERR_INVALID_ARG, RTCI2C_TAG, "SCL start period must be greater than 8 RTC_FAST_CLK cycles");
    ESP_RETURN_ON_FALSE((scl_low_period + scl_high_period - sda_duty_period) > 8, ESP_ERR_INVALID_ARG, RTCI2C_TAG, "SCL low + SCL high - SDA duty must be greater than 8 RTC_FAST_CLK cycles");

    /* Verify SDA duty num constraints */
    ESP_RETURN_ON_FALSE(sda_duty_period > 14, ESP_ERR_INVALID_ARG, RTCI2C_TAG, "SDA duty period must be greater than 14 RTC_FAST_CLK cycles");

    /* Set the RTC I2C timing parameters */
    i2c_dev->i2c_scl_low.val = scl_low_period;              // SCL low period
    i2c_dev->i2c_scl_high.val = scl_high_period;            // SCL high period
    i2c_dev->i2c_sda_duty.val = sda_duty_period;            // SDA duty cycle
    i2c_dev->i2c_scl_start_period.val = scl_start_period;   // Wait time after START condition
    i2c_dev->i2c_scl_stop_period.val = scl_stop_period;     // Wait time before END condition
    i2c_dev->i2c_to.val = i2c_trans_timeout;                // I2C transaction timeout

    return ESP_OK;
}

void ulp_riscv_i2c_reset() {
    /* Clear any stale config registers */
    WRITE_PERI_REG(RTC_I2C_CTRL_REG, 0);
    WRITE_PERI_REG(SENS_SAR_I2C_CTRL_REG, 0);

    /* Reset RTC I2C */
    SET_PERI_REG_MASK(SENS_SAR_PERI_RESET_CONF_REG, SENS_RTC_I2C_RESET);
    i2c_dev->i2c_ctrl.i2c_i2c_reset = 1;
    esp_rom_delay_us(20);
    i2c_dev->i2c_ctrl.i2c_i2c_reset = 0;
    CLEAR_PERI_REG_MASK(SENS_SAR_PERI_RESET_CONF_REG, SENS_RTC_I2C_RESET);
}

esp_err_t ulp_riscv_i2c_master_init(const ulp_riscv_i2c_cfg_t *cfg)
{
    ulp_riscv_i2c_reset();

    /* Verify that the input cfg param is valid */
    ESP_RETURN_ON_FALSE(cfg, ESP_ERR_INVALID_ARG, RTCI2C_TAG, "RTC I2C configuration is NULL");

    /* Configure RTC I2C GPIOs */
    ESP_RETURN_ON_ERROR(i2c_set_pin(cfg), RTCI2C_TAG, "Failed to configure RTC I2C GPIOs");

    /* For esp32s3, we need to enable the rtc_i2c clock gate before accessing rtc i2c registers */
    SET_PERI_REG_MASK(SENS_SAR_PERI_CLK_GATE_CONF_REG, SENS_RTC_I2C_CLK_EN);

    /* Configure the RTC I2C controller in master mode */
    i2c_dev->i2c_ctrl.i2c_ms_mode = 1;

    /* Enable RTC I2C Clock gate */
    i2c_dev->i2c_ctrl.i2c_i2c_ctrl_clk_gate_en = 1;

    /* Configure RTC I2C timing paramters */
    ESP_RETURN_ON_ERROR(i2c_set_timing(cfg), RTCI2C_TAG, "Failed to configure RTC I2C timing");

    return ESP_OK;
}