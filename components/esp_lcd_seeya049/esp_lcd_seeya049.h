/*
 * SPDX-FileCopyrightText: 2025 Sindarin Inc.
 *
 * Driver for SeeYA SY049WDM07 (0.49" micro-OLED, 1920x1080) via MIPI DSI
 */

#pragma once

#include "soc/soc_caps.h"

#ifdef __cplusplus
extern "C" {
#endif

#if SOC_MIPI_DSI_SUPPORTED
#include "esp_lcd_mipi_dsi.h"
#include "esp_lcd_panel_interface.h"
#include "esp_lcd_panel_vendor.h"

/**
 * @brief LCD panel initialization commands.
 *
 */
typedef struct {
    int cmd;               /*<! The specific LCD command */
    const void *data;      /*<! Buffer that holds the command specific data */
    size_t data_bytes;     /*<! Size of `data` in memory, in bytes */
    unsigned int delay_ms; /*<! Delay in milliseconds after this command */
} seeya049_lcd_init_cmd_t;

/**
 * @brief LCD panel vendor configuration.
 *
 * @note  This structure needs to be passed to the `esp_lcd_panel_dev_config_t::vendor_config`.
 *
 */
typedef struct {
    const seeya049_lcd_init_cmd_t
        *init_cmds; /*!< Pointer to initialization commands array. Set to NULL if using default
                     * commands. The array should be declared as `static const` and positioned
                     * outside the function. Please refer to `vendor_specific_init_default` in
                     * source file.
                     */
    uint16_t init_cmds_size; /*<! Number of commands in above array */
    struct {
        esp_lcd_dsi_bus_handle_t dsi_bus;             /*!< MIPI-DSI bus configuration */
        const esp_lcd_dpi_panel_config_t *dpi_config; /*!< MIPI-DPI panel configuration */
        uint8_t lane_num;                             /*!< Number of MIPI-DSI lanes */
    } mipi_config;
} seeya049_vendor_config_t;

/**
 * @brief Create LCD panel for model SeeYA SY049WDM07
 *
 * @note  Vendor specific initialization can be different between manufacturers, should consult the
 * LCD supplier for initialization sequence code.
 *
 * @param[in]  io LCD panel IO handle
 * @param[in]  panel_dev_config General panel device configuration
 * @param[out] ret_panel Returned LCD panel handle
 * @return
 *      - ESP_ERR_INVALID_ARG   if parameter is invalid
 *      - ESP_OK                on success
 *      - Otherwise             on fail
 */
esp_err_t esp_lcd_new_panel_seeya049(const esp_lcd_panel_io_handle_t io,
                                     const esp_lcd_panel_dev_config_t *panel_dev_config,
                                     esp_lcd_panel_handle_t *ret_panel);

esp_err_t panel_seeya049_send_init_cmds(esp_lcd_panel_t *panel);

esp_err_t panel_seeya049_all_pixels_on_off(esp_lcd_panel_t *panel, bool on_off);

/**
 * @brief Run the panel's built-in self test mode initialization sequence
 *
 * @param[in] panel LCD panel handle
 * @return
 *      - ESP_OK on success
 *      - ESP_ERR_INVALID_ARG if parameter is invalid
 *      - Otherwise on error
 */
esp_err_t panel_seeya049_run_selftest(esp_lcd_panel_t *panel);

/**
 * @brief Write raw data to the display RAM
 *
 * @param[in] panel LCD panel handle
 * @param[in] data Pointer to the data buffer to write
 * @param[in] length Length of data in bytes
 * @return
 *      - ESP_OK on success
 *      - ESP_ERR_INVALID_ARG if parameter is invalid
 *      - Otherwise on error
 */
esp_err_t panel_seeya049_write_ram(esp_lcd_panel_t *panel, const void *data, size_t length);

/**
 * @brief Dump the display's framebuffer over I2C using the RAMRD command
 *
 * This function will read the entire framebuffer (1920x1080x3 bytes) and print
 * the first 16 bytes for debugging purposes.
 *
 * @return
 *      - ESP_OK on success
 *      - ESP_ERR_NO_MEM if memory allocation fails
 *      - ESP_ERR_INVALID_ARG if parameter is invalid
 *      - Otherwise on error
 */
esp_err_t panel_seeya049_dump_framebuffer_i2c(void);

/**
 * @brief Move the display area of the panel
 *
 * @param[in] panel LCD panel handle
 * @param[in] dx Delta x
 * @param[in] dy Delta y
 */
esp_err_t panel_seeya049_move_display_area(esp_lcd_panel_t *panel, int16_t dx, int16_t dy);

#endif // SOC_MIPI_DSI_SUPPORTED

/**
 * @brief Run the panel's built-in self test mode initialization sequence using I2C
 *
 * This variant uses I2C communication instead of MIPI DSI, with SCL on pin 21 and SDA on pin 22.
 *
 * @return
 *      - ESP_OK on success
 *      - ESP_ERR_INVALID_ARG if parameter is invalid
 *      - Otherwise on error
 */
esp_err_t panel_seeya049_run_selftest_i2c(void);

/**
 * @brief Run the panel's framebuffer test using I2C
 *
 * This variant uses I2C communication instead of MIPI DSI, with SCL on pin 21 and SDA on pin 22.
 *
 * @param[in] data Pointer to the data buffer to write
 * @param[in] length Length of data in bytes
 * @return
 *      - ESP_OK on success
 *      - ESP_ERR_INVALID_ARG if parameter is invalid
 *      - Otherwise on error
 */
esp_err_t panel_seeya049_run_framebuffer_test_i2c(uint8_t *data, size_t length);

#ifdef __cplusplus
}
#endif
