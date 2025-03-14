/*
 * Driver for SeeYA SY049WDM07 (0.49" micro-OLED, 1920x1080) via MIPI DSI
 */

#include "esp_lcd_seeya049.h"

#include <string.h>

#include "driver/gpio.h"
#include "driver/i2c.h"
#include "esp_check.h"
#if SOC_MIPI_DSI_SUPPORTED
#include "esp_lcd_mipi_dsi.h"
#endif
#include "esp_lcd_panel_commands.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

// Mysteries
// 1. When flipping Y with MADCTL, the display colors get inverted
// 2. The RGB bit in MADCTL doesn't seem to have an effect
// 3. RAMWR doesn't seem to have an effect
// 4. Rendered gradients aren't smooth

// From the SY103WAM01 datasheet
// #define SEEYA049_CMD_SWRESET 0x01 // Software Reset
// #define SEEYA049_CMD_CMODE 0x03   // Compression Mode
// #define SEEYA049_CMD_SLPIN 0x10   // Sleep In
// #define SEEYA049_CMD_SLPOUT 0x11  // Sleep Out
// #define SEEYA049_CMD_ALLPOFF 0x22 // All Display Off
// #define SEEYA049_CMD_ALLPON 0x23  // All Display On
// #define SEEYA049_CMD_TCON 0x25    // Temperature Sensor Enable
// #define SEEYA049_CMD_DISPOFF 0x28 // Display Off
// #define SEEYA049_CMD_DISPON 0x29  // Display On
// #define SEEYA049_CMD_CASET 0x2A   // Column Address Set
// #define SEEYA049_CMD_RASET 0x2B   // Row Address Set
// #define SEEYA049_CMD_TEON 0x2C    // Tearing Effect Line On
// #define SEEYA049_CMD_MADCTL 0x36    // Memory Access Control
// #define SEEYA049_CMD_IDMOFF 0x38    // Idle Mode Off
// #define SEEYA049_CMD_IDMON 0x39     // Idle Mode On
// #define SEEYA049_CMD_COLMOD 0x3A    // Interface Pixel Format
// #define SEEYA049_CMD_WRDISBV 0x51 // Write Display Brightness
#define SEEYA049_CMD_WRCTRLD 0x53 // Write CTRL Display
// #define SEEYA049_CMD_SCACTRL 0x69   // Scaling Up Control
// #define SEEYA049_CMD_IFCONFG 0x6B   // Interface Configure
// #define SEEYA049_CMD_RESCTRL1 0x80  // Resolution Control1
// #define SEEYA049_CMD_RESCTRL2 0x81  // Resolution Control2
// #define SEEYA049_CMD_RESCTRL3 0x82  // Resolution Control3
// #define SEEYA049_CMD_PORCH_EXT 0x83 // Porch CTRL

// From the SY103WDM07 datasheet
#define SEEYA049_CMD_SWRESET 0x01 // Software Reset
#define SEEYA049_CMD_CMODE 0x03   // Compression Mode
#define SEEYA049_CMD_SLPIN 0x10   // Sleep In
#define SEEYA049_CMD_SLPOUT 0x11  // Sleep Out
#define SEEYA049_CMD_ALLPOFF 0x22 // All Pixels Off
#define SEEYA049_CMD_ALLPON 0x23  // All Pixels On
#define SEEYA049_CMD_DISPOFF 0x28 // Display Off
#define SEEYA049_CMD_DISPON 0x29  // Display On
#define SEEYA049_CMD_CASET 0x2A   // Column Address Set
#define SEEYA049_CMD_RASET 0x2B   // Row Address Set
#define SEEYA049_CMD_RAMWR 0x2C   // Memory Write
#define SEEYA049_CMD_RAMRD 0x2E   // Memory Read
// Note: the RGB bit in MADCTL doesn't seem to have an effect
#define SEEYA049_CMD_MADCTL 0x36        // Memory Access Control
#define SEEYA049_CMD_MADCTL_MY (1 << 0) // Mirror Y
#define SEEYA049_CMD_MADCTL_MX (1 << 1) // Mirror X
#define SEEYA049_CMD_IDMOFF 0x38        // Idle Mode Off
#define SEEYA049_CMD_IDMON 0x39         // Idle Mode On
// Scaling seems to work but causes some flickering
#define SEEYA049_CMD_SCACTRL 0x69 // Scaling Up Control
#define SEEYA049_CMD_IFCONFG 0x6B // Interface Configure (lane count)
#define SEEYA049_CMD_ANCTRL1 0x90 // Display Anchor On/Off
#define SEEYA049_CMD_ANCTRL2 0x91 // Display Anchor CTRL
#define SEEYA049_CMD_BCCTR3 0xC2  // Brightness Control

// From the SY103WDM01 datasheet (only the commands that are different from SY103WDM07)
// COLMOD is only documented as valid as 0x77 and doesn't seem to have an effect
#define SEEYA049_CMD_COLMOD 0x3A  // Interface Pixel Format
#define SEEYA049_CMD_WRDISBV 0x51 // Write Display Brightness

// I2C configuration
#define I2C_MASTER_SCL_IO 21 // SCL pin
#define I2C_MASTER_SDA_IO 22 // SDA pin
// #define I2C_DEVICE_ADDR 0x4C // I2C device address
#define I2C_DEVICE_ADDR 0x4E      // I2C device address
#define I2C_MASTER_NUM I2C_NUM_0  // I2C port number
#define I2C_MASTER_FREQ_HZ 400000 // I2C master clock frequency
#define I2C_TIMEOUT_MS 1000       // I2C timeout in milliseconds

typedef struct {
    esp_lcd_panel_io_handle_t io;
    int reset_gpio_num;
    uint8_t madctl_val; // save current value of SEEYA049_CMD_MADCTL register
    const seeya049_lcd_init_cmd_t *init_cmds;
    uint16_t init_cmds_size;
    uint8_t lane_num;
    struct {
        unsigned int reset_level : 1;
    } flags;
    // To save the original functions of MIPI DPI panel
    esp_err_t (*del)(esp_lcd_panel_t *panel);
    esp_err_t (*init)(esp_lcd_panel_t *panel);
} seeya049_panel_t;

static const char *TAG = "seeya049";

// NED-provided init sequence
// The only critical command from NED seems to be SEEYA049_CMD_WRCTRLD. Then we add
// SEEYA049_CMD_IFCONFG to make the display work in 2 lane mode.
static const seeya049_lcd_init_cmd_t vendor_specific_init_default[] = {
    // CMD1
    // WRCTRLD? Absolutely critical -- this must happen or nothing else works
    // Based on the SEEYA_SY103WAM01_Pre-spec V0.4.pdf datasheet here's what the bits mean:
    // D5: Brightness control enable (must be on)
    // D3: Display dimming control enable
    // D2: Temperature index dimming control enable
    // D0: Display mode control (1=High brightness, low contrast, 0=Low brightness, high contrast)
    {.cmd = SEEYA049_CMD_WRCTRLD, .data = (uint8_t[]){0x29}, .data_bytes = 1, .delay_ms = 0},

    // WRDISBV? Display brightness control? No change when removing
    {.cmd = SEEYA049_CMD_WRDISBV, .data = (uint8_t[]){0xFF, 0x01}, .data_bytes = 2, .delay_ms = 0},
    {.cmd = SEEYA049_CMD_CMODE, .data = (uint8_t[]){0x00}, .data_bytes = 1, .delay_ms = 0},
    // RESCTL?
    {.cmd = 0x80,
     .data = (uint8_t[]){0x00, 0xE0, 0xE0, 0x0E, 0x00, 0x31},
     .data_bytes = 6,
     .delay_ms = 0},
    {.cmd = 0x81,
     .data = (uint8_t[]){0x03, 0x04, 0x00, 0x29, 0x00, 0x05, 0x00},
     .data_bytes = 7,
     .delay_ms = 0},
    {.cmd = 0x82,
     .data = (uint8_t[]){0x03, 0x04, 0x00, 0x29, 0x00, 0x05, 0x01},
     .data_bytes = 7,
     .delay_ms = 0},
    // Tearing effect line on
    {.cmd = 0x35, .data = (uint8_t[]){0x00}, .data_bytes = 1, .delay_ms = 0},
    // Gamma set
    {.cmd = 0x26, .data = (uint8_t[]){0x20}, .data_bytes = 1, .delay_ms = 0},

    // CMD2 P1
    // No change in behavior removing these
    {.cmd = 0xF0, .data = (uint8_t[]){0xAA, 0x11}, .data_bytes = 2, .delay_ms = 0},
    {.cmd = 0xC0, .data = (uint8_t[]){0x00}, .data_bytes = 1, .delay_ms = 0},
    {.cmd = SEEYA049_CMD_BCCTR3,
     .data = (uint8_t[]){0x03, 0xFF, 0x03, 0xFF, 0x03, 0xFF, 0x03, 0xFF},
     .data_bytes = 8,
     .delay_ms = 0},

    // CMD2 P2
    // No change removing these
    {.cmd = 0xF0, .data = (uint8_t[]){0xAA, 0x12}, .data_bytes = 2, .delay_ms = 0},
    {.cmd = 0xBF, .data = (uint8_t[]){0x37, 0xA9}, .data_bytes = 2, .delay_ms = 0},

    // CMD3 P0
    // No change removing these
    {.cmd = 0xFF, .data = (uint8_t[]){0x5A, 0x80}, .data_bytes = 2, .delay_ms = 0},
    {.cmd = 0x65, .data = (uint8_t[]){0x2F}, .data_bytes = 1, .delay_ms = 0},
    {.cmd = 0xF2, .data = (uint8_t[]){0x01}, .data_bytes = 1, .delay_ms = 0},

    // CMD3 P1
    // No change removing these
    {.cmd = 0xFF, .data = (uint8_t[]){0x5A, 0x81}, .data_bytes = 2, .delay_ms = 0},
    {.cmd = 0x65, .data = (uint8_t[]){0x05}, .data_bytes = 1, .delay_ms = 0},
    {.cmd = 0xF2, .data = (uint8_t[]){0x22}, .data_bytes = 1, .delay_ms = 0},
    {.cmd = 0x65, .data = (uint8_t[]){0x0A}, .data_bytes = 1, .delay_ms = 0},
    {.cmd = 0xF2, .data = (uint8_t[]){0x00}, .data_bytes = 1, .delay_ms = 0},
    {.cmd = 0x65, .data = (uint8_t[]){0x17}, .data_bytes = 1, .delay_ms = 0},
    {.cmd = 0xF9,
     .data =
         (uint8_t[]){0x5E, 0x62, 0x66, 0x6A, 0x6F, 0x73, 0x77, 0x7B, 0x7F, 0x84, 0x88, 0x8C, 0x90},
     .data_bytes = 13,
     .delay_ms = 0},

    // Set to 2 lane MIPI mode
    {.cmd = SEEYA049_CMD_IFCONFG, .data = (uint8_t[]){0x02}, .data_bytes = 1, .delay_ms = 0},

    // Wake from sleep
    {.cmd = 0, .data = NULL, .data_bytes = 0, .delay_ms = 20},
    {.cmd = SEEYA049_CMD_SLPOUT, .data = NULL, .data_bytes = 0, .delay_ms = 100}, // Sleep Out
};

// Built-in self-test init sequence provided by NED
static const seeya049_lcd_init_cmd_t vendor_specific_init_selftest[] = {
    // CMD1
    {.cmd = SEEYA049_CMD_WRCTRLD, .data = (uint8_t[]){0x29}, .data_bytes = 1, .delay_ms = 0},
    {.cmd = SEEYA049_CMD_WRDISBV, .data = (uint8_t[]){0xFF, 0x01}, .data_bytes = 2, .delay_ms = 0},
    {.cmd = SEEYA049_CMD_CMODE, .data = (uint8_t[]){0x00}, .data_bytes = 1, .delay_ms = 0},
    {.cmd = 0x80,
     .data = (uint8_t[]){0x00, 0xE0, 0xE0, 0x0E, 0x00, 0x31},
     .data_bytes = 6,
     .delay_ms = 0},
    {.cmd = 0x81,
     .data = (uint8_t[]){0x03, 0x04, 0x00, 0x29, 0x00, 0x05, 0x00},
     .data_bytes = 7,
     .delay_ms = 0},
    {.cmd = 0x82,
     .data = (uint8_t[]){0x03, 0x04, 0x00, 0x29, 0x00, 0x05, 0x01},
     .data_bytes = 7,
     .delay_ms = 0},
    {.cmd = 0x35, .data = (uint8_t[]){0x00}, .data_bytes = 1, .delay_ms = 0},
    {.cmd = 0x26, .data = (uint8_t[]){0x20}, .data_bytes = 1, .delay_ms = 0},

    // CMD2 P1
    {.cmd = 0xF0, .data = (uint8_t[]){0xAA, 0x11}, .data_bytes = 2, .delay_ms = 0},
    {.cmd = 0xC0, .data = (uint8_t[]){0x00}, .data_bytes = 1, .delay_ms = 0},
    {.cmd = SEEYA049_CMD_BCCTR3,
     .data = (uint8_t[]){0x03, 0xFF, 0x03, 0xFF, 0x03, 0xFF, 0x03, 0xFF},
     .data_bytes = 8,
     .delay_ms = 0},

    // CMD2 P2
    {.cmd = 0xF0, .data = (uint8_t[]){0xAA, 0x12}, .data_bytes = 2, .delay_ms = 0},
    {.cmd = 0xBF, .data = (uint8_t[]){0x37, 0xA9}, .data_bytes = 2, .delay_ms = 0},

    // CMD3 P0
    {.cmd = 0xFF, .data = (uint8_t[]){0x5A, 0x80}, .data_bytes = 2, .delay_ms = 0},
    {.cmd = 0x65, .data = (uint8_t[]){0x2F}, .data_bytes = 1, .delay_ms = 0},
    {.cmd = 0xF2, .data = (uint8_t[]){0x01}, .data_bytes = 1, .delay_ms = 0},

    // CMD3 P1
    {.cmd = 0xFF, .data = (uint8_t[]){0x5A, 0x81}, .data_bytes = 2, .delay_ms = 0},
    {.cmd = 0x65, .data = (uint8_t[]){0x05}, .data_bytes = 1, .delay_ms = 0},
    {.cmd = 0xF2, .data = (uint8_t[]){0x22}, .data_bytes = 1, .delay_ms = 0},
    {.cmd = 0x65, .data = (uint8_t[]){0x0A}, .data_bytes = 1, .delay_ms = 0},
    {.cmd = 0xF2, .data = (uint8_t[]){0x00}, .data_bytes = 1, .delay_ms = 0},
    {.cmd = 0x65, .data = (uint8_t[]){0x17}, .data_bytes = 1, .delay_ms = 0},
    {.cmd = 0xF9,
     .data =
         (uint8_t[]){0x5E, 0x62, 0x66, 0x6A, 0x6F, 0x73, 0x77, 0x7B, 0x7F, 0x84, 0x88, 0x8C, 0x90},
     .data_bytes = 13,
     .delay_ms = 0},

    // Final configuration
    {.cmd = 0xF0, .data = (uint8_t[]){0xAA, 0x11}, .data_bytes = 2, .delay_ms = 0},
    {.cmd = 0xC5,
     .data = (uint8_t[]){0x01, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF},
     .data_bytes = 6,
     .delay_ms = 0},
    {.cmd = 0xC4, .data = (uint8_t[]){0xAA, 0x55, 0x01, 0x80}, .data_bytes = 4, .delay_ms = 0},
};

#if SOC_MIPI_DSI_SUPPORTED

// Forward declaration of panel methods
static esp_err_t panel_seeya049_del(esp_lcd_panel_t *panel);
static esp_err_t panel_seeya049_init(esp_lcd_panel_t *panel);
static esp_err_t panel_seeya049_reset(esp_lcd_panel_t *panel);
static esp_err_t panel_seeya049_mirror(esp_lcd_panel_t *panel, bool mirror_x, bool mirror_y);
static esp_err_t panel_seeya049_disp_on_off(esp_lcd_panel_t *panel, bool on_off);
static esp_err_t panel_seeya049_sleep(esp_lcd_panel_t *panel, bool sleep);

esp_err_t esp_lcd_new_panel_seeya049(const esp_lcd_panel_io_handle_t io,
                                     const esp_lcd_panel_dev_config_t *panel_dev_config,
                                     esp_lcd_panel_handle_t *ret_panel) {
    ESP_RETURN_ON_FALSE(io && panel_dev_config && ret_panel, ESP_ERR_INVALID_ARG, TAG,
                        "invalid arguments");
    seeya049_vendor_config_t *vendor_config =
        (seeya049_vendor_config_t *)panel_dev_config->vendor_config;
    ESP_RETURN_ON_FALSE(vendor_config && vendor_config->mipi_config.dpi_config &&
                            vendor_config->mipi_config.dsi_bus,
                        ESP_ERR_INVALID_ARG, TAG, "invalid vendor config");

    esp_err_t ret = ESP_OK;
    seeya049_panel_t *seeya049 = (seeya049_panel_t *)calloc(1, sizeof(seeya049_panel_t));
    ESP_RETURN_ON_FALSE(seeya049, ESP_ERR_NO_MEM, TAG, "no mem for seeya049 panel");

    if (panel_dev_config->reset_gpio_num >= 0) {
        gpio_config_t io_conf = {
            .mode = GPIO_MODE_OUTPUT_OD,
            .pull_up_en = GPIO_PULLUP_DISABLE,
            .pull_down_en = GPIO_PULLDOWN_DISABLE,
            .intr_type = GPIO_INTR_DISABLE,
            .pin_bit_mask = 1ULL << panel_dev_config->reset_gpio_num,
        };
        ESP_GOTO_ON_ERROR(gpio_config(&io_conf), err, TAG, "configure GPIO for RST line failed");
    }

    seeya049->io = io;
    seeya049->init_cmds = vendor_config->init_cmds;
    seeya049->init_cmds_size = vendor_config->init_cmds_size;
    seeya049->lane_num = vendor_config->mipi_config.lane_num;
    seeya049->reset_gpio_num = panel_dev_config->reset_gpio_num;
    seeya049->flags.reset_level = panel_dev_config->flags.reset_active_high;

    // Create MIPI DPI panel
    ESP_GOTO_ON_ERROR(esp_lcd_new_panel_dpi(vendor_config->mipi_config.dsi_bus,
                                            vendor_config->mipi_config.dpi_config, ret_panel),
                      err, TAG, "create MIPI DPI panel failed");
    ESP_LOGD(TAG, "new MIPI DPI panel @%p", *ret_panel);

    // Save the original functions of MIPI DPI panel
    seeya049->del = (*ret_panel)->del;
    seeya049->init = (*ret_panel)->init;
    // Overwrite the functions of MIPI DPI panel
    (*ret_panel)->del = panel_seeya049_del;
    (*ret_panel)->init = panel_seeya049_init;
    (*ret_panel)->reset = panel_seeya049_reset;
    (*ret_panel)->mirror = panel_seeya049_mirror;
    (*ret_panel)->disp_on_off = panel_seeya049_disp_on_off;
    (*ret_panel)->disp_sleep = panel_seeya049_sleep;
    (*ret_panel)->user_data = seeya049;
    ESP_LOGD(TAG, "new seeya049 panel @%p", seeya049);

    return ESP_OK;

err:
    if (seeya049) {
        if (panel_dev_config->reset_gpio_num >= 0) {
            gpio_reset_pin(panel_dev_config->reset_gpio_num);
        }
        free(seeya049);
    }
    return ret;
}

static esp_err_t panel_seeya049_del(esp_lcd_panel_t *panel) {
    ESP_LOGI(TAG, "panel_seeya049_del");
    seeya049_panel_t *seeya049 = (seeya049_panel_t *)panel->user_data;

    if (seeya049->reset_gpio_num >= 0) {
        // Reset display
        ESP_RETURN_ON_ERROR(panel_seeya049_reset(panel), TAG, "reset display failed");

        gpio_reset_pin(seeya049->reset_gpio_num);
    }
    // Delete MIPI DPI panel
    seeya049->del(panel);
    ESP_LOGI(TAG, "del seeya049 panel @%p", seeya049);
    free(seeya049);

    return ESP_OK;
}

// WARNING: Reading registers seems to break MIPI. This seems likely to be a ESP32-P4 driver
// issue. Perhaps here:
// https://github.com/espressif/esp-idf/blob/0d6099ec533c4b647fb7a7b0b8942bc7aeb82f90/components/hal/mipi_dsi_hal.c#L202
// since the MIPI driver exits video mode when reading registers.

esp_err_t panel_seeya049_log_registers(esp_lcd_panel_t *panel) {
    seeya049_panel_t *seeya049 = (seeya049_panel_t *)panel->user_data;
    esp_lcd_panel_io_handle_t io = seeya049->io;

    uint8_t out8;
    uint8_t out8Array[2];
    uint16_t out16;
    esp_err_t err;

    // Read SEEYA049_CMD_WRCTRLD
    err = esp_lcd_panel_io_rx_param(io, SEEYA049_CMD_WRCTRLD, &out8, 1);
    ESP_RETURN_ON_ERROR(err, TAG, "read register WRCTRLD failed");
    ESP_LOGI(TAG, "WRCTRLD register (0x%x): 0x%x", SEEYA049_CMD_WRCTRLD, out8);

    // Read SEEYA049_CMD_WRDISBV
    err = esp_lcd_panel_io_rx_param(io, SEEYA049_CMD_WRDISBV, &out8Array, 2);
    ESP_RETURN_ON_ERROR(err, TAG, "read register WRDISBV failed");
    ESP_LOGI(TAG, "WRDISBV register (0x%x): 0x%x 0x%x", SEEYA049_CMD_WRDISBV, out8Array[0],
             out8Array[1]);

    // Read CMODE
    err = esp_lcd_panel_io_rx_param(io, SEEYA049_CMD_CMODE, &out8, 1);
    ESP_RETURN_ON_ERROR(err, TAG, "read register CMODE failed");
    ESP_LOGI(TAG, "CMODE register (0x%x): 0x%x", SEEYA049_CMD_CMODE, out8);

    // Read CASET
    err = esp_lcd_panel_io_rx_param(io, SEEYA049_CMD_CASET, &out16, 2);
    ESP_RETURN_ON_ERROR(err, TAG, "read register CASET failed");
    ESP_LOGI(TAG, "CASET register (0x%x): 0x%x", SEEYA049_CMD_CASET, out16);

    // Read RASET
    err = esp_lcd_panel_io_rx_param(io, SEEYA049_CMD_RASET, &out16, 2);
    ESP_RETURN_ON_ERROR(err, TAG, "read register RASET failed");
    ESP_LOGI(TAG, "RASET register (0x%x): 0x%x", SEEYA049_CMD_RASET, out16);

    // Read MADCTL
    err = esp_lcd_panel_io_rx_param(io, SEEYA049_CMD_MADCTL, &out8, 1);
    ESP_RETURN_ON_ERROR(err, TAG, "read register MADCTL failed");
    ESP_LOGI(TAG, "MADCTL register (0x%x): 0x%x", SEEYA049_CMD_MADCTL, out8);

    // Read IDMOFF
    err = esp_lcd_panel_io_rx_param(io, SEEYA049_CMD_IDMOFF, &out8, 1);
    ESP_RETURN_ON_ERROR(err, TAG, "read register IDMOFF failed");
    ESP_LOGI(TAG, "IDMOFF register (0x%x): 0x%x", SEEYA049_CMD_IDMOFF, out8);

    // Read IDMON
    err = esp_lcd_panel_io_rx_param(io, SEEYA049_CMD_IDMON, &out8, 1);
    ESP_RETURN_ON_ERROR(err, TAG, "read register IDMON failed");
    ESP_LOGI(TAG, "IDMON register (0x%x): 0x%x", SEEYA049_CMD_IDMON, out8);

    // Read SCACTRL
    err = esp_lcd_panel_io_rx_param(io, SEEYA049_CMD_SCACTRL, &out8, 1);
    ESP_RETURN_ON_ERROR(err, TAG, "read register SCACTRL failed");
    ESP_LOGI(TAG, "SCACTRL register (0x%x): 0x%x", SEEYA049_CMD_SCACTRL, out8);

    // Read IFCONFG
    err = esp_lcd_panel_io_rx_param(io, SEEYA049_CMD_IFCONFG, &out8, 1);
    ESP_RETURN_ON_ERROR(err, TAG, "read register IFCONFG failed");
    ESP_LOGI(TAG, "IFCONFG register (0x%x): 0x%x", SEEYA049_CMD_IFCONFG, out8);

    // Read ANCTRL1
    err = esp_lcd_panel_io_rx_param(io, SEEYA049_CMD_ANCTRL1, &out8, 1);
    ESP_RETURN_ON_ERROR(err, TAG, "read register ANCTRL1 failed");
    ESP_LOGI(TAG, "ANCTRL1 register (0x%x): 0x%x", SEEYA049_CMD_ANCTRL1, out8);

    // Read ANCTRL2
    err = esp_lcd_panel_io_rx_param(io, SEEYA049_CMD_ANCTRL2, &out8, 1);
    ESP_RETURN_ON_ERROR(err, TAG, "read register ANCTRL2 failed");
    ESP_LOGI(TAG, "ANCTRL2 register (0x%x): 0x%x", SEEYA049_CMD_ANCTRL2, out8);

    // Read BCCTR3
    uint8_t bcctrl3_val[8];
    err = esp_lcd_panel_io_rx_param(io, SEEYA049_CMD_BCCTR3, bcctrl3_val, 8);
    ESP_RETURN_ON_ERROR(err, TAG, "read register BCCTR3 failed");
    ESP_LOGI(TAG, "BCCTR3 register (0x%x): 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x",
             SEEYA049_CMD_BCCTR3, bcctrl3_val[0], bcctrl3_val[1], bcctrl3_val[2], bcctrl3_val[3],
             bcctrl3_val[4], bcctrl3_val[5], bcctrl3_val[6], bcctrl3_val[7]);

    return ESP_OK;
}

esp_err_t panel_seeya049_send_init_cmds(esp_lcd_panel_t *panel) {
    seeya049_panel_t *seeya049 = (seeya049_panel_t *)panel->user_data;
    esp_lcd_panel_io_handle_t io = seeya049->io;
    const seeya049_lcd_init_cmd_t *init_cmds = NULL;
    uint16_t init_cmds_size = 0;

    // vendor specific initialization, it can be different between manufacturers
    // should consult the LCD supplier for initialization sequence code
    if (seeya049->init_cmds) {
        init_cmds = seeya049->init_cmds;
        init_cmds_size = seeya049->init_cmds_size;
    } else {
        init_cmds = vendor_specific_init_default;
        init_cmds_size = sizeof(vendor_specific_init_default) / sizeof(seeya049_lcd_init_cmd_t);
    }

    for (int i = 0; i < init_cmds_size; i++) {
        if (init_cmds[i].cmd != 0) {
            switch (init_cmds[i].cmd) {
            case SEEYA049_CMD_MADCTL:
                seeya049->madctl_val = ((uint8_t *)init_cmds[i].data)[0];
                break;
            default:
                break;
            }

            ESP_LOGI(TAG, "esp_lcd_panel_io_tx_param %02Xh", init_cmds[i].cmd);
            // Send command
            ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, init_cmds[i].cmd, init_cmds[i].data,
                                                          init_cmds[i].data_bytes),
                                TAG, "send command failed");
        }
        if (init_cmds[i].delay_ms > 0) {
            ESP_LOGI(TAG, "delay %dms", init_cmds[i].delay_ms);
            vTaskDelay(pdMS_TO_TICKS(init_cmds[i].delay_ms));
        }
    }

    // WARNING: May break MIPI. See comment above this method
    // panel_seeya049_log_registers(panel);

    return ESP_OK;
}

static esp_err_t panel_seeya049_init(esp_lcd_panel_t *panel) {
    seeya049_panel_t *seeya049 = (seeya049_panel_t *)panel->user_data;
    ESP_RETURN_ON_ERROR(panel_seeya049_send_init_cmds(panel), TAG, "send init commands failed");

    // What happens in here? See:
    // https://github.com/espressif/esp-idf/blob/0d6099ec533c4b647fb7a7b0b8942bc7aeb82f90/components/esp_lcd/dsi/esp_lcd_panel_dpi.c#L403
    ESP_RETURN_ON_ERROR(seeya049->init(panel), TAG, "init MIPI DPI panel failed");

    return ESP_OK;
}

static esp_err_t panel_seeya049_reset(esp_lcd_panel_t *panel) {
    ESP_LOGI(TAG, "panel_seeya049_reset");
    seeya049_panel_t *seeya049 = (seeya049_panel_t *)panel->user_data;
    esp_lcd_panel_io_handle_t io = seeya049->io;

    // Perform hardware reset
    if (seeya049->reset_gpio_num >= 0) {
        gpio_set_level(seeya049->reset_gpio_num, seeya049->flags.reset_level);
        vTaskDelay(pdMS_TO_TICKS(1));
        gpio_set_level(seeya049->reset_gpio_num, !seeya049->flags.reset_level);
        vTaskDelay(pdMS_TO_TICKS(120));
    } else if (io) { // Perform software reset
        ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, SEEYA049_CMD_SWRESET, NULL, 0), TAG,
                            "send command failed");
        vTaskDelay(pdMS_TO_TICKS(120));
    }

    return ESP_OK;
}

static esp_err_t panel_seeya049_mirror(esp_lcd_panel_t *panel, bool mirror_x, bool mirror_y) {
    seeya049_panel_t *seeya049 = (seeya049_panel_t *)panel->user_data;
    esp_lcd_panel_io_handle_t io = seeya049->io;

    uint8_t madctl_val = seeya049->madctl_val;

    if (mirror_x) {
        madctl_val |= SEEYA049_CMD_MADCTL_MX;
    } else {
        madctl_val &= ~SEEYA049_CMD_MADCTL_MX;
    }
    // NOTE: Mirroring the Y axis seems to mess with the pixel format so colors get all weird
    // Red -> Yellow, Blue -> Cyan, Green -> Magenta
    // These are inverted colors, but simply sending the inverse of the colors doesn't seem to work.
    if (mirror_y) {
        madctl_val |= SEEYA049_CMD_MADCTL_MY;
    } else {
        madctl_val &= ~SEEYA049_CMD_MADCTL_MY;
    }

    ESP_LOGI(TAG, "Setting MADCTL to 0x%x", madctl_val);
    ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, SEEYA049_CMD_MADCTL, &madctl_val, 1), TAG,
                        "send command failed");

    seeya049->madctl_val = madctl_val;

    return ESP_OK;
}

static esp_err_t panel_seeya049_disp_on_off(esp_lcd_panel_t *panel, bool on_off) {
    ESP_LOGI(TAG, "panel_seeya049_disp_on_off %d", on_off);

    seeya049_panel_t *seeya049 = (seeya049_panel_t *)panel->user_data;
    esp_lcd_panel_io_handle_t io = seeya049->io;

    if (on_off) {
        ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, SEEYA049_CMD_DISPON, NULL, 0), TAG,
                            "send command failed");
        vTaskDelay(pdMS_TO_TICKS(20));
        // In the NED sequence these happen after display on though they don't seem to be necessary
        // Seems to work without these set
        // ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, 0xF0, (uint8_t[]){0xAA, 0x11}, 2), TAG,
        //                     "send command failed");
        // ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, 0xC0, (uint8_t[]){0xFF}, 1), TAG,
        //                     "send command failed");
    } else {
        ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, SEEYA049_CMD_DISPOFF, NULL, 0), TAG,
                            "send command failed");
    }
    return ESP_OK;
}

static esp_err_t panel_seeya049_sleep(esp_lcd_panel_t *panel, bool sleep) {
    ESP_LOGI(TAG, "panel_seeya049_sleep");

    seeya049_panel_t *seeya049 = (seeya049_panel_t *)panel->user_data;
    esp_lcd_panel_io_handle_t io = seeya049->io;

    if (sleep) {
        ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, SEEYA049_CMD_SLPIN, NULL, 0), TAG,
                            "send command failed");
    } else {
        ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, SEEYA049_CMD_SLPOUT, NULL, 0), TAG,
                            "send command failed");
    }
    vTaskDelay(pdMS_TO_TICKS(100));

    return ESP_OK;
}

esp_err_t panel_seeya049_all_pixels_on_off(esp_lcd_panel_t *panel, bool on_off) {
    ESP_LOGI(TAG, "panel_seeya049_all_pixels_on_off %d", on_off);

    seeya049_panel_t *seeya049 = (seeya049_panel_t *)panel->user_data;
    esp_lcd_panel_io_handle_t io = seeya049->io;

    if (on_off) {
        ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, SEEYA049_CMD_ALLPON, NULL, 0), TAG,
                            "send command failed");
    } else {
        ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, SEEYA049_CMD_ALLPOFF, NULL, 0), TAG,
                            "send command failed");
    }

    return ESP_OK;
}

esp_err_t panel_seeya049_run_selftest(esp_lcd_panel_t *panel) {
    ESP_RETURN_ON_FALSE(panel, ESP_ERR_INVALID_ARG, TAG, "parameter is invalid");

    ESP_LOGI(TAG, "panel_seeya049_run_selftest");
    seeya049_panel_t *seeya049 = (seeya049_panel_t *)panel->user_data;
    esp_lcd_panel_io_handle_t io = seeya049->io;

    // First reset the panel to ensure we're in a known state
    ESP_RETURN_ON_ERROR(panel_seeya049_reset(panel), TAG, "reset display failed");

    // Run through the self-test initialization sequence
    const seeya049_lcd_init_cmd_t *init_cmds = vendor_specific_init_selftest;
    uint16_t init_cmds_size =
        sizeof(vendor_specific_init_selftest) / sizeof(seeya049_lcd_init_cmd_t);

    for (int i = 0; i < init_cmds_size; i++) {
        if (init_cmds[i].cmd != 0) {
            ESP_LOGI(TAG, "send command %02Xh", init_cmds[i].cmd);
            // Send command
            ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, init_cmds[i].cmd, init_cmds[i].data,
                                                          init_cmds[i].data_bytes),
                                TAG, "send command failed");
        }
        if (init_cmds[i].delay_ms > 0) {
            ESP_LOGI(TAG, "delay %dms", init_cmds[i].delay_ms);
            vTaskDelay(pdMS_TO_TICKS(init_cmds[i].delay_ms));
        }
    }

    return ESP_OK;
}

esp_err_t panel_seeya049_write_ram(esp_lcd_panel_t *panel, const void *data, size_t length) {
    ESP_RETURN_ON_FALSE(panel && data && length > 0, ESP_ERR_INVALID_ARG, TAG, "invalid argument");

    seeya049_panel_t *seeya049 = (seeya049_panel_t *)panel->user_data;
    esp_lcd_panel_io_handle_t io = seeya049->io;

    // Send the RAM write command followed by the data
    // NOTE: There might be a better way to do this
    ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, SEEYA049_CMD_RAMWR, data, length), TAG,
                        "write display RAM failed");

    // This needs to happen otherwise the screen is black
    ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, 0xF0, (uint8_t[]){0xAA, 0x11}, 2), TAG,
                        "write 0xF0 failed");

    // Turn on self-test. This seems to control which tests get run
    // ESP_RETURN_ON_ERROR(
    //     esp_lcd_panel_io_tx_param(io, 0xC5, (uint8_t[]){0x01, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}, 6),
    //     TAG, "write 0xC5 failed");

    // Turn on framebuffer display? Turns on screen full white but I can't get patterns to update
    // Byte1: 0xAA -- if set to 0x22 I get a black/off screen
    // Byte2: 0x55 -- if set to 0x11 I get a black/off screen
    // Byte3: 0x01 -- if set to 0x00 i get a black/off screen
    // Byte4: 0x80 -- if set to 0x00 or 0x02 i get black/off screen
    // ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, 0xC4, (uint8_t[]){0xAA, 0x55, 0x01, 0x80},
    // 4), TAG, "write 0xC4 failed");

    // Try to write again?
    ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, SEEYA049_CMD_RAMWR, data, length), TAG,
                        "write display RAM failed");

    return ESP_OK;
}

#endif // SOC_MIPI_DSI_SUPPORTED

static esp_err_t panel_seeya049_i2c_init(void) {
    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = I2C_MASTER_SDA_IO,
        .scl_io_num = I2C_MASTER_SCL_IO,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = I2C_MASTER_FREQ_HZ,
    };

    esp_err_t err = i2c_param_config(I2C_MASTER_NUM, &conf);
    if (err != ESP_OK) return err;

    return i2c_driver_install(I2C_MASTER_NUM, conf.mode, 0, 0, 0);
}

static esp_err_t panel_seeya049_i2c_deinit(void) { return i2c_driver_delete(I2C_MASTER_NUM); }

static esp_err_t panel_seeya049_i2c_write_reg(uint16_t reg_addr, uint16_t data) {
    uint8_t write_buf[4];
    write_buf[0] = (reg_addr >> 8) & 0xFF; // Register address high byte
    write_buf[1] = reg_addr & 0xFF;        // Register address low byte
    write_buf[2] = (data >> 8) & 0xFF;     // Data high byte
    write_buf[3] = data & 0xFF;            // Data low byte

    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (I2C_DEVICE_ADDR << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write(cmd, write_buf, sizeof(write_buf), true);
    i2c_master_stop(cmd);

    esp_err_t ret = i2c_master_cmd_begin(I2C_MASTER_NUM, cmd, pdMS_TO_TICKS(I2C_TIMEOUT_MS));
    i2c_cmd_link_delete(cmd);

    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to write to register 0x%04X: %d", reg_addr, ret);
    }

    return ret;
}

static esp_err_t panel_seeya049_disp_on_off_i2c(bool on_off) {
    if (on_off) {
        ESP_RETURN_ON_ERROR(panel_seeya049_i2c_write_reg(SEEYA049_CMD_DISPON, 1), TAG,
                            "Failed to write display on/off command");

        vTaskDelay(pdMS_TO_TICKS(20));
    } else {
        ESP_RETURN_ON_ERROR(panel_seeya049_i2c_write_reg(SEEYA049_CMD_DISPOFF, 0), TAG,
                            "Failed to write display on/off command");
    }

    return ESP_OK;
}

static esp_err_t panel_seeya049_i2c_write_ram(uint8_t *data, size_t length) {
    uint8_t write_buf[2];
    const uint16_t reg_addr = 0x2C00;
    write_buf[0] = (reg_addr >> 8) & 0xFF; // Register address high byte
    write_buf[1] = reg_addr & 0xFF;        // Register address low byte

    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (I2C_DEVICE_ADDR << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write(cmd, write_buf, sizeof(write_buf), false);
    i2c_master_write(cmd, data, length, true);
    i2c_master_stop(cmd);

    esp_err_t ret = i2c_master_cmd_begin(
        I2C_MASTER_NUM, cmd, pdMS_TO_TICKS((length + 3) * 10 / I2C_MASTER_FREQ_HZ * 1000));
    i2c_cmd_link_delete(cmd);

    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to write to ram 0x%04X: %s", reg_addr, esp_err_to_name(ret));
    }

    return ret;
}

esp_err_t panel_seeya049_send_init_cmds_i2c(const seeya049_lcd_init_cmd_t *init_cmds,
                                            uint16_t size) {
    // Process each command in the sequence
    for (int i = 0; i < size; i++) {
        if (init_cmds[i].cmd != 0) {
            ESP_LOGI(TAG, "Processing command 0x%02X", init_cmds[i].cmd);

            // For each command, we need to write its data bytes to sequential registers
            for (int j = 0; j < init_cmds[i].data_bytes; j++) {
                uint16_t reg_addr = (init_cmds[i].cmd << 8) | j;
                uint16_t data_byte = ((uint8_t *)init_cmds[i].data)[j];

                ESP_RETURN_ON_ERROR(panel_seeya049_i2c_write_reg(reg_addr, data_byte), TAG,
                                    "Failed to write command data");
            }
        }

        if (init_cmds[i].delay_ms > 0) {
            ESP_LOGI(TAG, "Delay %dms", init_cmds[i].delay_ms);
            vTaskDelay(pdMS_TO_TICKS(init_cmds[i].delay_ms));
        }
    }

    return ESP_OK;
}

esp_err_t panel_seeya049_run_selftest_i2c(void) {
    ESP_LOGI(TAG, "Initializing I2C for SeeYa049 self-test");

    // Initialize I2C
    ESP_RETURN_ON_ERROR(panel_seeya049_i2c_init(), TAG, "I2C master initialization failed");

    // Get the self-test initialization sequence
    ESP_RETURN_ON_ERROR(panel_seeya049_send_init_cmds_i2c(vendor_specific_init_selftest,
                                                          sizeof(vendor_specific_init_selftest) /
                                                              sizeof(seeya049_lcd_init_cmd_t)),
                        TAG, "Failed to send init commands");

    // Clean up I2C driver
    ESP_RETURN_ON_ERROR(panel_seeya049_i2c_deinit(), TAG, "I2C driver deletion failed");

    return ESP_OK;
}

esp_err_t panel_seeya049_run_framebuffer_test_i2c(uint8_t *data, size_t length) {

    // Initialize I2C
    ESP_RETURN_ON_ERROR(panel_seeya049_i2c_init(), TAG, "I2C master initialization failed");

    // Init sequence
    ESP_RETURN_ON_ERROR(panel_seeya049_send_init_cmds_i2c(vendor_specific_init_default,
                                                          sizeof(vendor_specific_init_default) /
                                                              sizeof(seeya049_lcd_init_cmd_t)),
                        TAG, "Failed to send init commands");

    ESP_RETURN_ON_ERROR(panel_seeya049_disp_on_off_i2c(true), TAG, "Failed to turn on display");

    ESP_LOGI(TAG, "Writing test pattern to screen");

    ESP_RETURN_ON_ERROR(panel_seeya049_i2c_write_ram(data, length), TAG,
                        "Failed to write test pattern");

    ESP_LOGI(TAG, "Write test pattern completed");

    // Clean up I2C driver
    ESP_RETURN_ON_ERROR(panel_seeya049_i2c_deinit(), TAG, "I2C driver deletion failed");

    return ESP_OK;
}

static esp_err_t panel_seeya049_i2c_read_reg(uint16_t reg_addr, uint16_t *data) {
    uint8_t write_buf[2];
    uint8_t read_buf[2];
    write_buf[0] = (reg_addr >> 8) & 0xFF; // Register address high byte
    write_buf[1] = reg_addr & 0xFF;        // Register address low byte

    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    // Write register address
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (I2C_DEVICE_ADDR << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write(cmd, write_buf, sizeof(write_buf), true);

    // Read data
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (I2C_DEVICE_ADDR << 1) | I2C_MASTER_READ, true);
    i2c_master_read(cmd, read_buf, sizeof(read_buf), I2C_MASTER_LAST_NACK);
    i2c_master_stop(cmd);

    esp_err_t ret = i2c_master_cmd_begin(I2C_MASTER_NUM, cmd, pdMS_TO_TICKS(I2C_TIMEOUT_MS));
    i2c_cmd_link_delete(cmd);

    if (ret == ESP_OK) {
        *data = (read_buf[0] << 8) | read_buf[1];
    } else {
        ESP_LOGE(TAG, "Failed to read from register 0x%04X: %d", reg_addr, ret);
    }

    return ret;
}

esp_err_t panel_seeya049_i2c_read_ram(uint8_t *data, size_t length) {
    ESP_RETURN_ON_FALSE(data && length > 0, ESP_ERR_INVALID_ARG, TAG, "invalid argument");

    const uint16_t reg_addr = SEEYA049_CMD_RAMRD << 8; // RAMRD command in high byte
    uint8_t write_buf[2];
    write_buf[0] = (reg_addr >> 8) & 0xFF; // Register address high byte (RAMRD command)
    write_buf[1] = reg_addr & 0xFF;        // Register address low byte (0)

    // First write the RAMRD command
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (I2C_DEVICE_ADDR << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write(cmd, write_buf, sizeof(write_buf), true);

    // Then read the data in chunks to avoid buffer overflow
    const size_t chunk_size = 1024; // Read 1KB at a time
    size_t bytes_read = 0;

    while (bytes_read < length) {
        size_t current_chunk =
            (length - bytes_read) < chunk_size ? (length - bytes_read) : chunk_size;

        // Start a new read transaction
        i2c_master_start(cmd);
        i2c_master_write_byte(cmd, (I2C_DEVICE_ADDR << 1) | I2C_MASTER_READ, true);

        // Read all but last byte with ACK
        if (current_chunk > 1) {
            i2c_master_read(cmd, &data[bytes_read], current_chunk - 1, I2C_MASTER_ACK);
        }
        // Read last byte with NACK
        i2c_master_read_byte(cmd, &data[bytes_read + current_chunk - 1], I2C_MASTER_NACK);
        i2c_master_stop(cmd);

        esp_err_t ret = i2c_master_cmd_begin(
            I2C_MASTER_NUM, cmd,
            pdMS_TO_TICKS((current_chunk + 3) * 10 / I2C_MASTER_FREQ_HZ * 1000));

        if (ret != ESP_OK) {
            i2c_cmd_link_delete(cmd);
            ESP_LOGE(TAG, "Failed to read RAM at offset %d: %s", bytes_read, esp_err_to_name(ret));
            return ret;
        }

        bytes_read += current_chunk;
        ESP_LOGI(TAG, "Read %d bytes of RAM (%d%%)", bytes_read, (bytes_read * 100) / length);
    }

    i2c_cmd_link_delete(cmd);
    return ESP_OK;
}

// Add function declaration at the top of the file with other declarations
esp_err_t panel_seeya049_dump_framebuffer_i2c(void) {
    ESP_LOGI(TAG, "Initializing I2C for SeeYa049 framebuffer dump");

    // Initialize I2C
    ESP_RETURN_ON_ERROR(panel_seeya049_i2c_init(), TAG, "I2C master initialization failed");

    // Calculate buffer size (1920x1080x3 bytes for 24-bit color)
    const size_t buffer_size = 1920 * 1080 * 3;
    uint8_t *buffer = heap_caps_malloc(buffer_size, MALLOC_CAP_DMA);
    if (!buffer) {
        panel_seeya049_i2c_deinit();
        return ESP_ERR_NO_MEM;
    }

    ESP_LOGI(TAG, "Reading framebuffer (%d bytes)...", buffer_size);
    esp_err_t ret = panel_seeya049_i2c_read_ram(buffer, buffer_size);

    if (ret == ESP_OK) {
        // Log first few bytes for debugging
        ESP_LOGI(TAG, "First 16 bytes of framebuffer:");
        for (int i = 0; i < 16; i++) {
            printf("%02x ", buffer[i]);
            if ((i + 1) % 8 == 0) printf("\n");
        }
    }

    // Free buffer
    heap_caps_free(buffer);

    // Clean up I2C driver
    ESP_RETURN_ON_ERROR(panel_seeya049_i2c_deinit(), TAG, "I2C driver deletion failed");

    return ret;
}

// TODO:
// Test disp/on/off to understand if i need to send any data with it (probably i don't?)
// Try to send some of the self test commands after writing the framebuffer
// Try to turn on self test then blast some stuff to the framebuffer
// Try to send variations of 0xC4 to see if I can get the screen to turn on from i2c
// Try to read some registers with i2c
// Try different combinations of ack when writing ram
// Try to read the framebuffer with i2c
// Try to read the framebuffer while self-test is running
