/*
 * Driver for SeeYA SY049WDM07 (0.49" micro-OLED, 1920x1080) via MIPI DSI
 */

#include "esp_lcd_seeya049.h"

#include <string.h>

#if SOC_MIPI_DSI_SUPPORTED
#include "driver/gpio.h"
#include "esp_check.h"
#include "esp_lcd_mipi_dsi.h"
#include "esp_lcd_panel_commands.h"
#include "esp_lcd_panel_interface.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

// SY103WAM01
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
#define SEEYA049_CMD_WRDISBV 0x51 // Write Display Brightness
#define SEEYA049_CMD_WRCTRLD 0x53 // Write CTRL Display
// #define SEEYA049_CMD_SCACTRL 0x69   // Scaling Up Control
// #define SEEYA049_CMD_IFCONFG 0x6B   // Interface Configure
// #define SEEYA049_CMD_RESCTRL1 0x80  // Resolution Control1
// #define SEEYA049_CMD_RESCTRL2 0x81  // Resolution Control2
// #define SEEYA049_CMD_RESCTRL3 0x82  // Resolution Control3
// #define SEEYA049_CMD_PORCH_EXT 0x83 // Porch CTRL

// SY103WDM07 (TODO: Is it the same as SY049WDM01?)
#define SEEYA049_CMD_SWRESET 0x01 // Software Reset
#define SEEYA049_CMD_CMODE 0x03   // Compression Mode
#define SEEYA049_CMD_SLPIN 0x10   // Sleep In
#define SEEYA049_CMD_SLPOUT 0x11  // Sleep Out
#define SEEYA049_CMD_ALLPOFF 0x22 // All Display Off
#define SEEYA049_CMD_ALLPON 0x23  // All Display On
#define SEEYA049_CMD_DISPOFF 0x28 // Display Off
#define SEEYA049_CMD_DISPON 0x29  // Display On
#define SEEYA049_CMD_CASET 0x2A   // Column Address Set
#define SEEYA049_CMD_RASET 0x2B   // Row Address Set
#define SEEYA049_CMD_RAMWR 0x2C   // Memory Write
#define SEEYA049_CMD_RAMRD 0x2E   // Memory Read
#define SEEYA049_CMD_MADCTL 0x36  // Memory Access Control
#define SEEYA049_CMD_IDMOFF 0x38  // Idle Mode Off
#define SEEYA049_CMD_IDMON 0x39   // Idle Mode On
#define SEEYA049_CMD_SCACTRL 0x69 // Scaling Up Control
#define SEEYA049_CMD_IFCONFG 0x6B // Interface Configure
#define SEEYA049_CMD_ANCTRL1 0x90 // Display Anchor On/Off
#define SEEYA049_CMD_ANCTRL2 0x91 // Display Anchor CTRL
#define SEEYA049_CMD_BCCTR3 0xC2  // Brightness Control

typedef struct {
    esp_lcd_panel_io_handle_t io;
    int reset_gpio_num;
    // uint8_t madctl_val; // save current value of LCD_CMD_MADCTL register
    // uint8_t colmod_val; // save surrent value of LCD_CMD_COLMOD register
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

    // switch (panel_dev_config->rgb_ele_order) {
    // case LCD_RGB_ELEMENT_ORDER_RGB:
    //     seeya049->madctl_val = 0;
    //     break;
    // case LCD_RGB_ELEMENT_ORDER_BGR:
    //     ili9881c->madctl_val |= LCD_CMD_BGR_BIT;
    //     break;
    // default:
    //     ESP_GOTO_ON_FALSE(false, ESP_ERR_NOT_SUPPORTED, err, TAG, "unsupported color space");
    //     break;
    // }

    // switch (panel_dev_config->bits_per_pixel) {
    // case 16: // RGB565
    //     ili9881c->colmod_val = 0x55;
    //     break;
    // case 18: // RGB666
    //     ili9881c->colmod_val = 0x66;
    //     break;
    // case 24: // RGB888
    //     ili9881c->colmod_val = 0x77;
    //     break;
    // default:
    //     ESP_GOTO_ON_FALSE(false, ESP_ERR_NOT_SUPPORTED, err, TAG, "unsupported pixel width");
    //     break;
    // }

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

static const seeya049_lcd_init_cmd_t vendor_specific_init_default[] = {
    // CMD1
    // Critical -- this must happen or nothing else works
    // Based on the SEEYA_SY103WAM01_Pre-spec V0.4.pdf datasheet here's what the bits mean:
    // D5: Brightness control enable (must be on)
    // D3: Display dimming control enable
    // D2: Temperature index dimming control enable
    // D0: Display mode control (1=High brightness, low contrast, 0=Low brightness, high contrast)
    {.cmd = SEEYA049_CMD_WRCTRLD, .data = (uint8_t[]){0x20}, .data_bytes = 1, .delay_ms = 0},

    // WRDISBV? Display brightness control? No change when removing
    // {.cmd = SEEYA049_CMD_WRDISBV, .data = (uint8_t[]){0x01, 0x00}, .data_bytes = 2, .delay_ms =
    // 0},
    // {.cmd = SEEYA049_CMD_CMODE, .data = (uint8_t[]){0x00}, .data_bytes = 1, .delay_ms = 0},
    // RESCTL? Removing these gives some kinda funky color bars unless i change the porch settings
    // away from NED's defaults (maybe timing related?)
    // {.cmd = 0x80,
    //  .data = (uint8_t[]){0x00, 0xE0, 0xE0, 0x0E, 0x00, 0x31},
    //  .data_bytes = 6,
    //  .delay_ms = 0},
    // {.cmd = 0x81,
    //  .data = (uint8_t[]){0x03, 0x04, 0x00, 0x29, 0x00, 0x05, 0x00},
    //  .data_bytes = 7,
    //  .delay_ms = 0},
    // {.cmd = 0x82,
    //  .data = (uint8_t[]){0x03, 0x04, 0x00, 0x29, 0x00, 0x05, 0x01},
    //  .data_bytes = 7,
    //  .delay_ms = 0},
    // Tearing effect line on
    // {.cmd = 0x35, .data = (uint8_t[]){0x00}, .data_bytes = 1, .delay_ms = 0},
    // Gamma set
    // {.cmd = 0x26, .data = (uint8_t[]){0x20}, .data_bytes = 1, .delay_ms = 0},
    // Nothing on the screen if this is omitted, so I'm confident it's working
    {.cmd = SEEYA049_CMD_IFCONFG, .data = (uint8_t[]){0x02}, .data_bytes = 1, .delay_ms = 0},
    // Setting to RGB mode does not seem to have an effect
    // {.cmd = SEEYA049_CMD_MADCTL, .data = (uint8_t[]){0x08}, .data_bytes = 1, .delay_ms = 0},

    // CMD2 P1
    // No change in behavior removing these
    // {.cmd = 0xF0, .data = (uint8_t[]){0xAA, 0x11}, .data_bytes = 2, .delay_ms = 0},
    // {.cmd = 0xC0, .data = (uint8_t[]){0x00}, .data_bytes = 1, .delay_ms = 0},
    // {.cmd = SEEYA049_CMD_BCCTR3,
    //  .data = (uint8_t[]){0x03, 0xFF, 0x03, 0xFF, 0x03, 0xFF, 0x03, 0xFF},
    //  .data_bytes = 8,
    //  .delay_ms = 0},

    // CMD2 P2
    // Background sometimes goes yellow when removing these
    // {.cmd = 0xF0, .data = (uint8_t[]){0xAA, 0x12}, .data_bytes = 2, .delay_ms = 0},
    // {.cmd = 0xBF, .data = (uint8_t[]){0x37, 0xA9}, .data_bytes = 2, .delay_ms = 0},

    // CMD3 P0
    // Background sometimes goes red when removing these
    // {.cmd = 0xFF, .data = (uint8_t[]){0x5A, 0x80}, .data_bytes = 2, .delay_ms = 0},
    // {.cmd = 0x65, .data = (uint8_t[]){0x2F}, .data_bytes = 1, .delay_ms = 0},
    // {.cmd = 0xF2, .data = (uint8_t[]){0x01}, .data_bytes = 1, .delay_ms = 20},

    // CMD3 P1
    // No change removing these
    // {.cmd = 0xFF, .data = (uint8_t[]){0x5A, 0x81}, .data_bytes = 2, .delay_ms = 0},
    // {.cmd = 0x65, .data = (uint8_t[]){0x05}, .data_bytes = 1, .delay_ms = 0},
    // {.cmd = 0xF2, .data = (uint8_t[]){0x22}, .data_bytes = 1, .delay_ms = 0},
    // {.cmd = 0x65, .data = (uint8_t[]){0x0A}, .data_bytes = 1, .delay_ms = 0},
    // {.cmd = 0xF2, .data = (uint8_t[]){0x00}, .data_bytes = 1, .delay_ms = 0},
    // {.cmd = 0x65, .data = (uint8_t[]){0x17}, .data_bytes = 1, .delay_ms = 0},
    // {.cmd = 0xF9,
    //  .data =
    //      (uint8_t[]){0x5E, 0x62, 0x66, 0x6A, 0x6F, 0x73, 0x77, 0x7B, 0x7F, 0x84, 0x88, 0x8C,
    //      0x90},
    //  .data_bytes = 13,
    //  .delay_ms = 20},

    // {.cmd = SEEYA049_CMD_ALLPON, .data = NULL, .data_bytes = 0, .delay_ms = 0},

    // Delay and final commands
    // Make sure to wait 20ms
    {.cmd = 0, .data = NULL, .data_bytes = 0, .delay_ms = 20},
    {.cmd = SEEYA049_CMD_SLPOUT, .data = NULL, .data_bytes = 0, .delay_ms = 100}, // Sleep Out
    // {.cmd = SEEYA049_CMD_DISPON, .data = NULL, .data_bytes = 0, .delay_ms = 20},  // Display On

    // // All pixels on
    // {.cmd = SEEYA049_CMD_ALLPON, .data = NULL, .data_bytes = 0, .delay_ms = 2000},
    // {.cmd = SEEYA049_CMD_ALLPOFF, .data = NULL, .data_bytes = 0, .delay_ms = 2000},

    // Final configuration
    // Seems to work without these set
    // {.cmd = 0xF0, .data = (uint8_t[]){0xAA, 0x11}, .data_bytes = 2, .delay_ms = 0},
    // {.cmd = 0xC0, .data = (uint8_t[]){0xFF}, .data_bytes = 1, .delay_ms = 0},
};

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

static esp_err_t panel_seeya049_init(esp_lcd_panel_t *panel) {
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

    uint8_t madctl_val = 0x00;

    if (mirror_x) {
        madctl_val |= 0x01;
    }
    if (mirror_y) {
        madctl_val |= 0x02;
    }

    ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, SEEYA049_CMD_MADCTL, &madctl_val, 1), TAG,
                        "send command failed");

    // Read back the MADCTL register
    uint8_t madctl_val_read;
    ESP_RETURN_ON_ERROR(esp_lcd_panel_io_rx_param(io, SEEYA049_CMD_MADCTL, &madctl_val_read, 1),
                        TAG, "read register MADCTL failed");
    ESP_LOGI(TAG, "MADCTL register: 0x%x", madctl_val_read);

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

#endif // SOC_MIPI_DSI_SUPPORTED
