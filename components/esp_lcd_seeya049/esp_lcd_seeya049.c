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
#define SEEYA049_CMD_IFCONFG 0x68 // Interface Configure
#define SEEYA049_CMD_ANCTRL1 0x90 // Display Anchor On/Off
#define SEEYA049_CMD_ANCTRL2 0x91 // Display Anchor CTRL
#define SEEYA049_CMD_BCCTRL3 0xC2 // Brightness Control

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
    // Software Reset
    {.cmd = SEEYA049_CMD_SWRESET,
     .data = NULL,
     .data_bytes = 0,
     .delay_ms = 120}, // Takes power consumption to ~2mA (from 1mA)

    // Set to 2 lane mode
    {.cmd = SEEYA049_CMD_IFCONFG,
     .data = (uint8_t[]){0xFF, 0xFF},
     .data_bytes = 2,
     .delay_ms = 100},

    // Compression Mode setting
    {.cmd = SEEYA049_CMD_CMODE, .data = (uint8_t[]){0x00, 0x00}, .data_bytes = 2, .delay_ms = 100},

    // Address Mode
    {.cmd = SEEYA049_CMD_MADCTL, .data = (uint8_t[]){0x00, 0x00}, .data_bytes = 2, .delay_ms = 100},

    // Sleep Out
    {.cmd = SEEYA049_CMD_SLPOUT,
     .data = NULL,
     .data_bytes = 0,
     .delay_ms = 200}, // Takes power consumption to ~12mA

    // // Idle mode off
    // {.cmd = SEEYA049_CMD_IDMOFF, .data = NULL, .data_bytes = 0, .delay_ms = 100},

    // All pixels on
    {.cmd = SEEYA049_CMD_ALLPON, .data = NULL, .data_bytes = 0, .delay_ms = 100},

    // // Display ON
    // {.cmd = SEEYA049_CMD_DISPON, .data = NULL, .data_bytes = 0, .delay_ms = 200},
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
        ESP_LOGI(TAG, "send command %02Xh", init_cmds[i].cmd);
        // Send command
        ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, init_cmds[i].cmd, init_cmds[i].data,
                                                      init_cmds[i].data_bytes),
                            TAG, "send command failed");
        vTaskDelay(pdMS_TO_TICKS(init_cmds[i].delay_ms));
    }

    uint16_t out;

    // Read CMODE
    esp_err_t err = esp_lcd_panel_io_rx_param(io, SEEYA049_CMD_CMODE, &out, 2);
    ESP_RETURN_ON_ERROR(err, TAG, "read register CMODE failed");
    ESP_LOGI(TAG, "CMODE register: 0x%x", out);

    // Read CASET
    err = esp_lcd_panel_io_rx_param(io, SEEYA049_CMD_CASET, &out, 2);
    ESP_RETURN_ON_ERROR(err, TAG, "read register CASET failed");
    ESP_LOGI(TAG, "CASET register: 0x%x", out);

    // Read RASET
    err = esp_lcd_panel_io_rx_param(io, SEEYA049_CMD_RASET, &out, 2);
    ESP_RETURN_ON_ERROR(err, TAG, "read register RASET failed");
    ESP_LOGI(TAG, "RASET register: 0x%x", out);

    // Read MADCTL
    err = esp_lcd_panel_io_rx_param(io, SEEYA049_CMD_MADCTL, &out, 2);
    ESP_RETURN_ON_ERROR(err, TAG, "read register MADCTL failed");
    ESP_LOGI(TAG, "MADCTL register: 0x%x", out);

    // Read SCACTL
    err = esp_lcd_panel_io_rx_param(io, SEEYA049_CMD_SCACTRL, &out, 2);
    ESP_RETURN_ON_ERROR(err, TAG, "read register SCACTL failed");
    ESP_LOGI(TAG, "SCACTRL register: 0x%x", out);

    // Read IFCONFG
    err = esp_lcd_panel_io_rx_param(io, SEEYA049_CMD_IFCONFG, &out, 2);
    ESP_RETURN_ON_ERROR(err, TAG, "read register IFCONFG failed");
    ESP_LOGI(TAG, "IFCONFG register: 0x%x", out);

    // Read ANCTRL1
    err = esp_lcd_panel_io_rx_param(io, SEEYA049_CMD_ANCTRL1, &out, 2);
    ESP_RETURN_ON_ERROR(err, TAG, "read register ANCTRL1 failed");
    ESP_LOGI(TAG, "ANCTRL1 register: 0x%x", out);

    ESP_LOGD(TAG, "send init commands success");

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
        vTaskDelay(pdMS_TO_TICKS(20));
    } else if (io) { // Perform software reset
        ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, SEEYA049_CMD_SWRESET, NULL, 0), TAG,
                            "send command failed");
        vTaskDelay(pdMS_TO_TICKS(20));
    }

    return ESP_OK;
}

static esp_err_t panel_seeya049_mirror(esp_lcd_panel_t *panel, bool mirror_x, bool mirror_y) {
    return ESP_OK;
}

static esp_err_t panel_seeya049_disp_on_off(esp_lcd_panel_t *panel, bool on_off) {
    ESP_LOGI(TAG, "panel_seeya049_disp_on_off %d", on_off);

    seeya049_panel_t *seeya049 = (seeya049_panel_t *)panel->user_data;
    esp_lcd_panel_io_handle_t io = seeya049->io;

    if (on_off) {
        ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, SEEYA049_CMD_DISPON, NULL, 0), TAG,
                            "send command failed");
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
