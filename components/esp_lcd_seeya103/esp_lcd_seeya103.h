/*
 * SPDX-FileCopyrightText: 2025 Sindarin Inc.
 *
 * Driver for SeeYA SY103WAM01 (1.03" micro-OLED, 2560x2560) via MIPI DSI
 */

#pragma once

#include <memory.h>

#include "soc/soc_caps.h"

#ifdef __cplusplus
extern "C" {
#endif

#define SEEYA103_I2C_ADDR 0x4C

// From the SY103WAM01 datasheet
#define SEEYA103_CMD_SWRESET 0x01        // Software Reset
#define SEEYA103_CMD_CMODE 0x03          // Compression Mode
#define SEEYA103_CMD_SLPIN 0x10          // Sleep In
#define SEEYA103_CMD_SLPOUT 0x11         // Sleep Out
#define SEEYA103_CMD_ALLPOFF 0x22        // All Display Off
#define SEEYA103_CMD_ALLPON 0x23         // All Display On
#define SEEYA103_CMD_TCON 0x25           // Temperature Sensor Enable
#define SEEYA103_CMD_GAMSET 0x26         // Gamma Set
#define SEEYA103_CMD_DISPOFF 0x28        // Display Off
#define SEEYA103_CMD_DISPON 0x29         // Display On
#define SEEYA103_CMD_CASET 0x2A          // Column Address Set
#define SEEYA103_CMD_RASET 0x2B          // Row Address Set
#define SEEYA103_CMD_TEON 0x35           // Tearing Effect Line On
#define SEEYA103_CMD_MADCTL 0x36         // Memory Access Control
#define SEEYA103_CMD_MADCTL_RGB (1 << 3) // Color-order of sub-pixels (doesn't seem to work)
#define SEEYA103_CMD_MADCTL_MY (1 << 0)  // Mirror Y
#define SEEYA103_CMD_MADCTL_MX (1 << 1)  // Mirror X
#define SEEYA103_CMD_IDMOFF 0x38         // Idle Mode Off
#define SEEYA103_CMD_IDMON 0x39          // Idle Mode On
#define SEEYA103_CMD_COLMOD 0x3A         // Interface Pixel Format
#define SEEYA103_CMD_WRDISBV 0x51        // Write Display Brightness
#define SEEYA103_CMD_WRCTRLD 0x53        // Write CTRL Display
#define SEEYA103_CMD_POINTER 0x65        // Adjust data pointer
#define SEEYA103_CMD_SCACTRL 0x69        // Scaling Up Control
#define SEEYA103_CMD_IFCONFG 0x6B        // Interface Configure
#define SEEYA103_CMD_RESCTRL1 0x80       // Resolution Control1
#define SEEYA103_CMD_RESCTRL2 0x81       // Resolution Control2
#define SEEYA103_CMD_RESCTRL3 0x82       // Resolution Control3
#define SEEYA103_CMD_PORCH_EXT 0x83      // Porch CTRL
// Inferring from the SY103WDM0* datasheet
#define SEEYA103_CMD_BCCTR3 0xC2 // Brightness Control

/**
 * @brief LCD panel initialization commands.
 *
 */
typedef struct {
    uint16_t cmd;          /*<! The specific LCD command */
    const void *data;      /*<! Buffer that holds the command specific data */
    size_t data_bytes;     /*<! Size of `data` in memory, in bytes */
    unsigned int delay_ms; /*<! Delay in milliseconds after this command */
} seeya103_lcd_init_cmd_t;

typedef enum {
    SEEYA103_SCALING_NONE = 0,
    SEEYA103_SCALING_2X = 1,
    SEEYA103_SCALING_1_33X = 2,
    SEEYA103_SCALING_DEFAULT = SEEYA103_SCALING_2X,
} seeya103_scaling_t;

#define SEEYA103_HRES_DEFAULT 1280
#define SEEYA103_VRES_DEFAULT 1280

static const seeya103_lcd_init_cmd_t seeya103_init_cmds[] = {
    // CMD1
    {.cmd = SEEYA103_CMD_CMODE, .data = (uint8_t[]){0x80}, .data_bytes = 1, .delay_ms = 0},
    {.cmd = SEEYA103_CMD_WRCTRLD, .data = (uint8_t[]){0x29}, .data_bytes = 1, .delay_ms = 0},
    {.cmd = SEEYA103_CMD_WRDISBV, .data = (uint8_t[]){0xFF, 0x01}, .data_bytes = 2, .delay_ms = 0},
    // 2X scaling by default
    {.cmd = SEEYA103_CMD_SCACTRL, .data = (uint8_t[]){0x01}, .data_bytes = 1, .delay_ms = 0},
    {.cmd = SEEYA103_CMD_IFCONFG, .data = (uint8_t[]){0x00}, .data_bytes = 1, .delay_ms = 0},
    {.cmd = SEEYA103_CMD_RESCTRL1,
     .data = (uint8_t[]){0x00, 0x40, 0x40, 0x11},
     .data_bytes = 4,
     .delay_ms = 0},
    {.cmd = SEEYA103_CMD_RESCTRL2,
     .data = (uint8_t[]){0x02, 0x14, 0x00, 0x0A, 0x00, 0x10, 0x00},
     .data_bytes = 7,
     .delay_ms = 0},
    {.cmd = SEEYA103_CMD_RESCTRL3,
     .data = (uint8_t[]){0x02, 0x14, 0x00, 0x0A, 0x00, 0x10, 0x00},
     .data_bytes = 7,
     .delay_ms = 0},
    {.cmd = SEEYA103_CMD_TEON, .data = (uint8_t[]){0x00}, .data_bytes = 1, .delay_ms = 0},
    {.cmd = SEEYA103_CMD_TCON, .data = (uint8_t[]){0x01}, .data_bytes = 1, .delay_ms = 0},
    {.cmd = SEEYA103_CMD_CASET, .data = (uint8_t[]){0x00, 0x00}, .data_bytes = 2, .delay_ms = 0},
    {.cmd = SEEYA103_CMD_RASET, .data = (uint8_t[]){0x00, 0x00}, .data_bytes = 2, .delay_ms = 0},

    // CMD2 P0
    // Not in the I2C commands
    // Not present in the 1920x1920 1.33x scaling
    {.cmd = 0xF0, .data = (uint8_t[]){0xAA, 0x10}, .data_bytes = 2, .delay_ms = 0},
    {.cmd = 0xB1, .data = (uint8_t[]){0x02}, .data_bytes = 1, .delay_ms = 0},

    // CMD2 P1
    {.cmd = 0xF0, .data = (uint8_t[]){0xAA, 0x11}, .data_bytes = 2, .delay_ms = 0},
    {.cmd = 0xC0, .data = (uint8_t[]){0x00, 0x04, 0x00}, .data_bytes = 3, .delay_ms = 0},
    {.cmd = SEEYA103_CMD_BCCTR3,
     .data = (uint8_t[]){0x03, 0xFF, 0x03, 0xFF, 0x03, 0xFF, 0x00, 0x90, 0x82},
     .data_bytes = 9,
     .delay_ms = 0},

    // CMD2,P2
    {.cmd = 0xF0, .data = (uint8_t[]){0xAA, 0x12}, .data_bytes = 2, .delay_ms = 0},
    {.cmd = 0xBF, .data = (uint8_t[]){0x37, 0xBE}, .data_bytes = 2, .delay_ms = 0},

    // CMD3 P1
    {.cmd = 0xFF, .data = (uint8_t[]){0x5A, 0x81}, .data_bytes = 2, .delay_ms = 0},
    // This adjusts the register pointer for the 0xF9 command
    {.cmd = SEEYA103_CMD_POINTER, .data = (uint8_t[]){0x0B}, .data_bytes = 1, .delay_ms = 0},
    {.cmd = 0xF9,
     .data =
         (uint8_t[]){0x58, 0x5F, 0x66, 0x6D, 0x74, 0x7B, 0x82, 0x89, 0x90, 0x97, 0x9E, 0xA5, 0xAC},
     .data_bytes = 13,
     .delay_ms = 0},

    // Wake from sleep
    {.cmd = 0, .data = NULL, .data_bytes = 0, .delay_ms = 20},
    {.cmd = SEEYA103_CMD_SLPOUT, .data = NULL, .data_bytes = 0, .delay_ms = 100},
};

static const seeya103_lcd_init_cmd_t seeya103_init_cmds_selftest[] = {
    // CMD1
    {.cmd = SEEYA103_CMD_CMODE, .data = (uint8_t[]){0x80}, .data_bytes = 1, .delay_ms = 0},
    {.cmd = SEEYA103_CMD_WRCTRLD, .data = (uint8_t[]){0x29}, .data_bytes = 1, .delay_ms = 0},
    {.cmd = SEEYA103_CMD_WRDISBV, .data = (uint8_t[]){0xFF, 0x01}, .data_bytes = 2, .delay_ms = 0},
    {.cmd = SEEYA103_CMD_SCACTRL, .data = (uint8_t[]){0x00}, .data_bytes = 1, .delay_ms = 0},
    {.cmd = SEEYA103_CMD_IFCONFG, .data = (uint8_t[]){0x00}, .data_bytes = 1, .delay_ms = 0},
    {.cmd = SEEYA103_CMD_RESCTRL1,
     .data = (uint8_t[]){0x01, 0x40, 0x40, 0x11},
     .data_bytes = 4,
     .delay_ms = 0},
    {.cmd = SEEYA103_CMD_RESCTRL2,
     .data = (uint8_t[]){0x02, 0x14, 0x00, 0x0A, 0x00, 0x10, 0x00},
     .data_bytes = 7,
     .delay_ms = 0},
    {.cmd = SEEYA103_CMD_RESCTRL3,
     .data = (uint8_t[]){0x02, 0x14, 0x00, 0x0A, 0x00, 0x10, 0x00},
     .data_bytes = 7,
     .delay_ms = 0},
    {.cmd = SEEYA103_CMD_TEON, .data = (uint8_t[]){0x00}, .data_bytes = 1, .delay_ms = 0},
    {.cmd = SEEYA103_CMD_TCON, .data = (uint8_t[]){0x01}, .data_bytes = 1, .delay_ms = 0},

    // CMD2 P1
    {.cmd = 0xF0, .data = (uint8_t[]){0xAA, 0x11}, .data_bytes = 2, .delay_ms = 0},
    {.cmd = 0xC0, .data = (uint8_t[]){0x00, 0x04, 0x00}, .data_bytes = 3, .delay_ms = 0},
    {.cmd = SEEYA103_CMD_BCCTR3,
     .data = (uint8_t[]){0x03, 0xFF, 0x03, 0xFF, 0x03, 0xFF, 0x00, 0x90, 0x82},
     .data_bytes = 9,
     .delay_ms = 0},

    // CMD2 P2
    {.cmd = 0xF0, .data = (uint8_t[]){0xAA, 0x12}, .data_bytes = 2, .delay_ms = 0},
    {.cmd = 0xBF, .data = (uint8_t[]){0x37, 0xBE}, .data_bytes = 2, .delay_ms = 0},

    // CMD3 P1
    {.cmd = 0xFF, .data = (uint8_t[]){0x5A, 0x81}, .data_bytes = 2, .delay_ms = 0},
    // This adjusts the register pointer for the 0xF9 command
    {.cmd = SEEYA103_CMD_POINTER, .data = (uint8_t[]){0x0B}, .data_bytes = 1, .delay_ms = 0},
    {.cmd = 0xF9,
     .data =
         (uint8_t[]){0x58, 0x5F, 0x66, 0x6D, 0x74, 0x7B, 0x82, 0x89, 0x90, 0x97, 0x9E, 0xA5, 0xAC},
     .data_bytes = 13,
     .delay_ms = 0},

    // Final configuration
    {.cmd = 0xF0, .data = (uint8_t[]){0xAA, 0x11}, .data_bytes = 2, .delay_ms = 0},
    // {.cmd = 0xC4, .data = (uint8_t[]){0xAA, 0x55, 0x01, 0x80}, .data_bytes = 4, .delay_ms = 0},
    {.cmd = 0xC4, .data = (uint8_t[]){0xAA, 0x55, 0x01, 0x80}, .data_bytes = 4, .delay_ms = 0},

    // Vertical bar pattern
    {.cmd = 0xC5, .data = (uint8_t[]){0x00, 0x80, 0xFF}, .data_bytes = 3, .delay_ms = 0},
    // Alternate test routine
    // First byte enables cycling through tests
    // {.cmd = 0xC5,
    //  .data = (uint8_t[]){0x01, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF},
    //  .data_bytes = 6,
    //  .delay_ms = 0},
};

#if SOC_MIPI_DSI_SUPPORTED
#include "esp_lcd_mipi_dsi.h"
#include "esp_lcd_panel_interface.h"
#include "esp_lcd_panel_vendor.h"

/**
 * @brief LCD panel vendor configuration.
 *
 * @note  This structure needs to be passed to the
 * `esp_lcd_panel_dev_config_t::vendor_config`.
 *
 */

typedef struct {
    int reset_gpio_num;
    const seeya103_lcd_init_cmd_t
        *init_cmds;          /*!< Pointer to initialization commands array. Set to NULL if using
                              * default commands. The array should be declared as `static const`
                              * and positioned outside the function. Please refer to
                              * `seeya103_init_cmds` in source file.
                              */
    uint16_t init_cmds_size; /*!< Number of commands in above array */
    uint16_t refresh_rate; /*!< Target refresh rate of the display. If set this will be used to help
                              calculate the dpi_clock_freq_mhz */
    uint16_t x_offset;     /*!< X offset of the display */
    uint16_t y_offset;     /*!< Y offset of the display */
    seeya103_scaling_t scaling;
    struct {
        esp_lcd_dsi_bus_handle_t dsi_bus;             /*!< MIPI-DSI bus configuration */
        const esp_lcd_dpi_panel_config_t *dpi_config; /*!< MIPI-DPI panel configuration */
        uint8_t lane_num;                             /*!< Number of MIPI-DSI lanes */
    } mipi_config;
} seeya103_vendor_config_t;

/**
 * @brief Create LCD panel for model SeeYA SY103WAM01
 *
 * @note  Vendor specific initialization can be different between manufacturers, should
 * consult the LCD supplier for initialization sequence code.
 *
 * @param[in]  io LCD panel IO handle
 * @param[in]  panel_dev_config General panel device configuration
 * @param[out] ret_panel Returned LCD panel handle
 * @return
 *      - ESP_ERR_INVALID_ARG   if parameter is invalid
 *      - ESP_OK                on success
 *      - Otherwise             on fail
 */
esp_err_t esp_lcd_new_panel_seeya103(const esp_lcd_panel_io_handle_t io,
                                     const esp_lcd_panel_dev_config_t *panel_dev_config,
                                     esp_lcd_panel_handle_t *ret_panel);

// Function pointer type for sending commands
typedef esp_err_t (*seeya103_send_cmd_fn_t)(seeya103_lcd_init_cmd_t cmd, void *user_data);

esp_err_t panel_seeya103_send_init_cmds(esp_lcd_panel_t *panel);
esp_err_t panel_seeya103_send_init_cmds_with_fn(const seeya103_lcd_init_cmd_t *init_cmds,
                                                uint16_t init_cmds_size, uint16_t width,
                                                uint16_t height, uint16_t x_offset,
                                                uint16_t y_offset, seeya103_scaling_t scaling,
                                                seeya103_send_cmd_fn_t send_cmd_fn,
                                                void *user_data);

esp_err_t panel_seeya103_all_pixels_on_off(esp_lcd_panel_t *panel, bool on_off);

/**
 * @brief Run the panel's built-in self test mode initialization sequence
 *
 * @param[in] panel LCD panel handle
 * @return
 *      - ESP_OK on success
 *      - ESP_ERR_INVALID_ARG if parameter is invalid
 *      - Otherwise on error
 */
esp_err_t panel_seeya103_run_selftest(esp_lcd_panel_t *panel);

/**
 * @brief Move the display area of the panel
 *
 * @param[in] panel LCD panel handle
 * @param[in] x_offset Delta x
 * @param[in] y_offset Delta y
 * @return
 *      - ESP_OK on success
 */
esp_err_t panel_seeya103_move_display_area(esp_lcd_panel_t *panel, int16_t x_offset,
                                           int16_t y_offset);

static inline esp_lcd_dsi_bus_config_t panel_seeya103_dsi_bus_config(void) {
    esp_lcd_dsi_bus_config_t bus_config = {
        .bus_id = 0,
        .num_data_lanes = 2,
        .phy_clk_src = MIPI_DSI_PHY_CLK_SRC_DEFAULT,
        // This doesn't seem to affect the size of what is drawn
        // lane_bit_rate_mbps / 8 / 18 must be > 1 and < 256
        .lane_bit_rate_mbps = 1000,
    };
    return bus_config;
}

static inline esp_lcd_dbi_io_config_t panel_seeya103_io_dbi_config(void) {
    esp_lcd_dbi_io_config_t dbi_config = {
        .virtual_channel = 0,
        .lcd_cmd_bits = 8,
        .lcd_param_bits = 16,
    };
    return dbi_config;
}

static inline esp_lcd_dpi_panel_config_t panel_seeya103_1280_1280_dpi_config(void) {
    esp_lcd_dpi_panel_config_t dpi_config = {
        .virtual_channel = 0,
        .dpi_clk_src = MIPI_DSI_DPI_CLK_SRC_DEFAULT,
        .dpi_clock_freq_mhz = 0, // Set by the refresh rate
        .pixel_format = LCD_COLOR_PIXEL_FORMAT_RGB888,
        .in_color_format = 0,
        .out_color_format = 0,
        .num_fbs = 1,
        .video_timing =
            {
                .h_size = 1280,
                .v_size = 1280,
                .hsync_pulse_width = 32,
                .hsync_back_porch = 64,
                .hsync_front_porch = 64,
                .vsync_pulse_width = 2,
                .vsync_back_porch = 8,
                .vsync_front_porch = 16,
            },
        .flags =
            {
                .use_dma2d = true,
                .disable_lp = false,
            },
    };
    return dpi_config;
}

static inline seeya103_vendor_config_t
panel_seeya103_vendor_config(esp_lcd_dsi_bus_handle_t mipi_dsi_bus,
                             esp_lcd_dpi_panel_config_t *dpi_config, int reset_gpio_num) {
    seeya103_vendor_config_t vendor_config = {
        .reset_gpio_num = reset_gpio_num,
        .init_cmds = seeya103_init_cmds,
        .init_cmds_size = sizeof(seeya103_init_cmds) / sizeof(seeya103_lcd_init_cmd_t),
        .refresh_rate = 60,
        .x_offset = 0,
        .y_offset = 0,
        .scaling = SEEYA103_SCALING_2X,
        .mipi_config =
            {
                .dsi_bus = mipi_dsi_bus,
                .dpi_config = dpi_config,
                .lane_num = 2,
            },
    };
    return vendor_config;
}

static inline esp_lcd_panel_dev_config_t
panel_seeya103_panel_config(seeya103_vendor_config_t *vendor_config) {
    const esp_lcd_panel_dev_config_t panel_config = {
        .reset_gpio_num = vendor_config->reset_gpio_num,
        // These don't have an effect on the test patterns
        .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_BGR,
        .data_endian = LCD_RGB_DATA_ENDIAN_BIG,
        .bits_per_pixel = 24,
        .flags =
            {
                .reset_active_high = false,
            },
        .vendor_config = vendor_config,
    };
    return panel_config;
}

#endif // SOC_MIPI_DSI_SUPPORTED

/**
 * @brief Initialize the I2C bus for the panel
 *
 * @return
 *      - ESP_OK on success
 *      - Otherwise on error
 */
esp_err_t panel_seeya103_i2c_init(int sda_gpio_num, int scl_gpio_num);

/**
 * @brief Deinitialize the I2C bus for the panel
 *
 * @return
 *      - ESP_OK on success
 *      - Otherwise on error
 */
esp_err_t panel_seeya103_i2c_deinit(void);

/**
 * @brief Write a register to the panel using I2C
 *
 * @param[in] reg_addr Register address
 * @param[in] data Data to write
 * @return
 */
esp_err_t panel_seeya103_i2c_write_reg(uint16_t reg_addr, uint16_t data);

/**
 * @brief Read a register from the panel using I2C
 *
 * @param[in] reg_addr Register address
 */
esp_err_t panel_seeya103_i2c_read_reg(uint16_t reg_addr, uint16_t *data);

/**
 * @brief Send initialization commands to the panel using I2C
 *
 * @param[in] width Width of the display
 * @param[in] height Height of the display
 * @param[in] x_offset X offset of the display
 * @param[in] y_offset Y offset of the display
 * @param[in] scaling Scaling factor
 * @param[in] init_cmds Pointer to the initialization commands array
 * @param[in] init_cmds_size Size of the initialization commands array
 * @return
 *      - ESP_OK on success
 *      - Otherwise on error
 */
esp_err_t panel_seeya103_send_cmds_i2c(uint16_t width, uint16_t height, uint16_t x_offset,
                                       uint16_t y_offset, seeya103_scaling_t scaling,
                                       const seeya103_lcd_init_cmd_t *init_cmds,
                                       uint16_t init_cmds_size);
/**
 * @brief
 *
 * @param on_off
 * @return esp_err_t
 */
esp_err_t panel_seeya103_disp_on_off_i2c(bool on_off);

/**
 * @brief Move the display area of the panel using I2C
 *
 * @param[in] x_offset X offset of the display
 * @param[in] y_offset Y offset of the display
 */
esp_err_t panel_seeya103_move_display_area_i2c(int16_t x_offset, int16_t y_offset);

#ifdef __cplusplus
}
#endif
