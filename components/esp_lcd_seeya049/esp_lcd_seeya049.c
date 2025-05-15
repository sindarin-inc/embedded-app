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
// 3. RAMWR doesn't seem to have an effect (SeeYa says the OSD feature is busted)
// 4. When setting RL_SEL high, it seems to cause that display to not render blues correctly. I
// think this is a bug in the hardware.
// When moving the display area, I can't draw outside the original draw box

// I2C configuration
#define I2C_MASTER_NUM I2C_NUM_0  // I2C port number
#define I2C_MASTER_FREQ_HZ 400000 // I2C master clock frequency
#define I2C_TIMEOUT_MS 1000       // I2C timeout in milliseconds

typedef struct {
    esp_lcd_panel_io_handle_t io;
    int reset_gpio_num;
    int width;
    int height;
    uint16_t x_offset;
    uint16_t y_offset;
    seeya049_scaling_t scaling;
    uint8_t madctl_val; // save current value of SEEYA049_CMD_MADCTL register
    const seeya049_lcd_init_cmd_t *init_cmds;
    uint16_t init_cmds_size;
    struct {
        unsigned int reset_level : 1;
    } flags;
    // To save the original functions of MIPI DPI panel
    esp_err_t (*del)(esp_lcd_panel_t *panel);
    esp_err_t (*init)(esp_lcd_panel_t *panel);
} seeya049_panel_t;

static const char *TAG = "seeya049";

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

    esp_lcd_dpi_panel_config_t dpi_config = *(vendor_config->mipi_config.dpi_config);
    seeya049->io = io;
    seeya049->init_cmds = vendor_config->init_cmds;
    seeya049->init_cmds_size = vendor_config->init_cmds_size;
    seeya049->reset_gpio_num = panel_dev_config->reset_gpio_num;
    seeya049->width = dpi_config.video_timing.h_size;
    seeya049->height = dpi_config.video_timing.v_size;
    seeya049->x_offset = vendor_config->x_offset;
    seeya049->y_offset = vendor_config->y_offset;
    seeya049->scaling = vendor_config->scaling;
    seeya049->flags.reset_level = panel_dev_config->flags.reset_active_high;

    // Derive the DPI clock frequency from the refresh rate if dpi_clock_freq_mhz is unset
    if (vendor_config->refresh_rate != 0 && dpi_config.dpi_clock_freq_mhz == 0) {
        uint32_t h_pixels =
            dpi_config.video_timing.h_size + dpi_config.video_timing.hsync_pulse_width +
            dpi_config.video_timing.hsync_back_porch + dpi_config.video_timing.hsync_front_porch;
        uint32_t v_pixels =
            dpi_config.video_timing.v_size + dpi_config.video_timing.vsync_pulse_width +
            dpi_config.video_timing.vsync_back_porch + dpi_config.video_timing.vsync_front_porch;
        uint32_t ideal_dpi_clock_hz = vendor_config->refresh_rate * h_pixels * v_pixels;

        // Max bandwidth for 2 lanes at 1000 mbps, accounting for MIPI overhead (GPT says this is up
        // to 30% but experimentally I'm getting success with 4%). This ends up to ~80MHz so we just
        // hardcode that.
        // uint32_t max_bandwidth = vendor_config->mipi_config.lane_num * 1000000000;
        // max_bandwidth = max_bandwidth - max_bandwidth * 0.04;
        // // 24 bits per pixel
        // uint32_t bits_per_pixel = 24;
        // uint32_t max_ideal_dpi_clock_hz = max_bandwidth / bits_per_pixel;
        if (ideal_dpi_clock_hz <= 80000000) {
            dpi_config.dpi_clock_freq_mhz = (ideal_dpi_clock_hz + 999999) / 1000000;
            ESP_LOGD(TAG,
                     "Overriding DPI clock to %" PRIu32 " MHz for refresh rate %" PRIu16
                     " Hz (ideal: %.2f MHz MIPI bandwidth: %" PRIu32 " mbps)",
                     dpi_config.dpi_clock_freq_mhz, vendor_config->refresh_rate,
                     ideal_dpi_clock_hz / 1000000.0, dpi_config.dpi_clock_freq_mhz * 24);
        } else {
            // Above 80 MHz, the ESP32-P4 can't keep up with the refresh rate
            dpi_config.dpi_clock_freq_mhz = 80;
            ESP_LOGD(TAG,
                     "Limiting DPI clock to %" PRIu32 " MHz for reduced refresh rate of %" PRIu16
                     " Hz (ideal: %.2f MHz MIPI bandwidth: %" PRIu32 " mbps)",
                     dpi_config.dpi_clock_freq_mhz, vendor_config->refresh_rate,
                     ideal_dpi_clock_hz / 1000000.0, dpi_config.dpi_clock_freq_mhz * 24);
        }
    }

    // Create MIPI DPI panel
    ESP_GOTO_ON_ERROR(
        esp_lcd_new_panel_dpi(vendor_config->mipi_config.dsi_bus, &dpi_config, ret_panel), err, TAG,
        "create MIPI DPI panel failed");
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

    // Log porch settings
    ESP_LOGD(TAG,
             "h_size: %" PRIu32 " hsync_pulse_width: %" PRIu32 " hsync_back_porch: %" PRIu32
             " hsync_front_porch: %" PRIu32,
             dpi_config.video_timing.h_size, dpi_config.video_timing.hsync_pulse_width,
             dpi_config.video_timing.hsync_back_porch, dpi_config.video_timing.hsync_front_porch);
    ESP_LOGD(TAG,
             "v_size: %" PRIu32 " vsync_pulse_width: %" PRIu32 " vsync_back_porch: %" PRIu32
             " vsync_front_porch: %" PRIu32,
             dpi_config.video_timing.v_size, dpi_config.video_timing.vsync_pulse_width,
             dpi_config.video_timing.vsync_back_porch, dpi_config.video_timing.vsync_front_porch);

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

    // vendor specific initialization, it can be different between manufacturers
    // should consult the LCD supplier for initialization sequence code
    if (seeya049->init_cmds == NULL || seeya049->init_cmds_size == 0) {
        ESP_LOGW(TAG, "No init commands provided");
        return ESP_OK;
    }

    uint8_t data[] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

    for (int i = 0; i < seeya049->init_cmds_size; i++) {
        seeya049_lcd_init_cmd_t cmd = seeya049->init_cmds[i];

        if (cmd.cmd != 0) {
            switch (cmd.cmd) {

            // Override the width and height if we're using a non-standard resolution
            case SEEYA049_CMD_RESCTRL1:
                if (seeya049->width != 1920 || seeya049->height != 1080 ||
                    seeya049->scaling != SEEYA049_SCALING_NONE) {
                    // This is undocumented in the 0.49" datasheet, but the 1.03" gives us enough
                    // clues to use it.
                    // Full resolution 1920x1080 (0x1E0*4 = 1920, 0x10E*4 = 1080)
                    // (uint8_t[]){0x00, 0xE0, 0xE0, 0x0E, 0x00, 0x31},
                    // Half resolution 960x540 (0xF0*4 = 960, 0x87*4 = 540)
                    // (uint8_t[]){0x00, 0xF0, 0xF0, 0x87, 0x00, 0x10},
                    // Doom resolution 320x200 (0x50*4 = 320, 0x32*4 = 200)
                    // (uint8_t[]){0x00, 0x50, 0x50, 0x32, 0x00, 0x00},

                    // X-axis resolution for input image size
                    int input_width = seeya049->width / 4;

                    // Display resolution for display image size
                    int display_width = seeya049->width;
                    int display_height = seeya049->height;

                    switch (seeya049->scaling) {
                    case SEEYA049_SCALING_1_33X:
                        // Multiply by 4/3 then divide by 4
                        display_width = display_width / 3;
                        display_height = display_height / 3;
                        break;
                    case SEEYA049_SCALING_1_5X:
                        // Multiply by 3/2 then divide by 4
                        display_width = display_width * 3 / 8;
                        display_height = display_height * 3 / 8;
                        break;
                    case SEEYA049_SCALING_2X:
                        // Multiply by 2 then divide by 4
                        display_width = display_width / 2;
                        display_height = display_height / 2;
                        break;
                    case SEEYA049_SCALING_NONE:
                    default:
                        // Default to 1/4 resolution
                        display_width = seeya049->width / 4;
                        display_height = seeya049->height / 4;
                        break;
                    }

                    data[0] = ((uint8_t *)cmd.data)[0];
                    data[1] = input_width & 0xFF;
                    data[2] = display_width & 0xFF;
                    data[3] = display_height & 0xFF;
                    data[4] = ((uint8_t *)cmd.data)[4];
                    data[5] = 0x00;

                    ESP_LOGD(TAG,
                             "Overriding resolution to: input_width=%d, display_width=%d, "
                             "display_height=%d",
                             input_width * 4, display_width * 4, display_height * 4);
                    if (input_width > 0xFF) {
                        data[5] |= 0x10;
                    }
                    if (display_width > 0xFF) {
                        data[5] |= 0x20;
                    }
                    if (display_height > 0xFF) {
                        data[5] |= 0x01;
                    }
                    cmd.data = &data;
                }
                break;
            case SEEYA049_CMD_CASET:
                if (seeya049->x_offset != 0) {
                    ESP_LOGD(TAG, "Overriding x_offset to %d", seeya049->x_offset);
                    uint16_t x_offset = seeya049->x_offset * 2 / 3;
                    data[0] = (x_offset >> 8) & 0xFF;
                    data[1] = x_offset & 0xFF;
                    cmd.data = &data;
                }
                break;
            case SEEYA049_CMD_RASET:
                if (seeya049->y_offset != 0) {
                    ESP_LOGD(TAG, "Overriding x_offset to %d", seeya049->x_offset);
                    // The datasheet seems to indicate this should be divided by 1.5, but
                    // experimentally dividing by 2 is more correct. I'm not sure why.
                    uint16_t y_offset = seeya049->y_offset / 2;
                    data[0] = (y_offset >> 8) & 0xFF;
                    data[1] = y_offset & 0xFF;
                    cmd.data = &data;
                }
                break;
            case SEEYA049_CMD_MADCTL:
                seeya049->madctl_val = ((uint8_t *)cmd.data)[0];
                break;
            case SEEYA049_CMD_SCACTRL:
                if (seeya049->scaling != SEEYA049_SCALING_NONE) {
                    ESP_LOGD(TAG, "Setting scaling to %d", seeya049->scaling);
                    data[0] = (uint8_t)seeya049->scaling;
                    cmd.data = &data;
                }
                break;
            default:
                break;
            }
            char data_str[64] = "";
            if (cmd.data && cmd.data_bytes > 0) {
                strcat(data_str, "data: ");
                for (int i = 0; i < cmd.data_bytes && i < 16; i++) {
                    sprintf(data_str + strlen(data_str), "0x%02X ", ((uint8_t *)cmd.data)[i]);
                }
            }
            ESP_LOGI(TAG, "MIPI TX: %02Xh %s", cmd.cmd, data_str);
            ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, cmd.cmd, cmd.data, cmd.data_bytes),
                                TAG, "send command failed");
        }
        if (cmd.delay_ms > 0) {
            ESP_LOGI(TAG, "delay %dms", cmd.delay_ms);
            vTaskDelay(pdMS_TO_TICKS(cmd.delay_ms));
        }
    }

    // WARNING: May break MIPI. See comment above this method
    // panel_seeya049_log_registers(panel);

    return ESP_OK;
}

static esp_err_t panel_seeya049_init(esp_lcd_panel_t *panel) {
    ESP_LOGI(TAG, "panel_seeya049_init");
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
        ESP_LOGD(TAG, "Reset pin %d to %d", seeya049->reset_gpio_num, seeya049->flags.reset_level);
        gpio_set_level(seeya049->reset_gpio_num, seeya049->flags.reset_level);
        vTaskDelay(pdMS_TO_TICKS(1));
        ESP_LOGD(TAG, "Reset pin %d to %d", seeya049->reset_gpio_num, !seeya049->flags.reset_level);
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
    // These are inverted colors, but simply sending the inverse of the colors doesn't seem to
    // work.
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
        ESP_LOGI(TAG, "MIPI TX: %02Xh", SEEYA049_CMD_DISPON);
        ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, SEEYA049_CMD_DISPON, NULL, 0), TAG,
                            "send command failed");
        vTaskDelay(pdMS_TO_TICKS(20));
        // In the NED sequence these happen after display on though they don't seem to be
        // necessary Seems to work without these set
        ESP_LOGI(TAG, "MIPI TX: 0xF0 data: 0xAA 0x11");
        ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, 0xF0, (uint8_t[]){0xAA, 0x11}, 2), TAG,
                            "send command failed");
        ESP_LOGI(TAG, "MIPI TX: 0xC0 data: 0xFF");
        ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, 0xC0, (uint8_t[]){0xFF}, 1), TAG,
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
    seeya049->init_cmds = seeya049_init_cmds_selftest;
    seeya049->init_cmds_size =
        sizeof(seeya049_init_cmds_selftest) / sizeof(seeya049_lcd_init_cmd_t);

    // First reset the panel to ensure we're in a known state
    ESP_RETURN_ON_ERROR(panel_seeya049_reset(panel), TAG, "reset display failed");

    panel_seeya049_send_init_cmds(panel);

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
    //     esp_lcd_panel_io_tx_param(io, 0xC5, (uint8_t[]){0x01, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF},
    //     6), TAG, "write 0xC5 failed");

    // Turn on framebuffer display? Turns on screen full white but I can't get patterns to
    // update Byte1: 0xAA -- if set to 0x22 I get a black/off screen Byte2: 0x55 -- if set to
    // 0x11 I get a black/off screen Byte3: 0x01 -- if set to 0x00 i get a black/off screen
    // Byte4: 0x80 -- if set to 0x00 or 0x02 i get black/off screen
    // ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, 0xC4, (uint8_t[]){0xAA, 0x55, 0x01,
    // 0x80}, 4), TAG, "write 0xC4 failed");

    // Try to write again?
    ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, SEEYA049_CMD_RAMWR, data, length), TAG,
                        "write display RAM failed");

    return ESP_OK;
}

esp_err_t panel_seeya049_move_display_area(esp_lcd_panel_t *panel, int16_t dx, int16_t dy) {
    ESP_LOGI(TAG, "panel_seeya049_move_display_area %d %d", dx, dy);

    seeya049_panel_t *seeya049 = (seeya049_panel_t *)panel->user_data;
    esp_lcd_panel_io_handle_t io = seeya049->io;

    dx = dx * 2 / 3;
    // The datasheet seems to indicate this should be divided by 1.5, but experimentally
    // dividing by 2 is more correct. I'm not sure why.
    dy = dy / 2;

    // Send the move display area command
    if (dx != -1) {
        uint8_t cmd_data[2];
        cmd_data[0] = (dx >> 8) & 0xFF; // High byte of dx
        cmd_data[1] = dx & 0xFF;        // Low byte of dx
        ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, SEEYA049_CMD_CASET, cmd_data, 2), TAG,
                            "Failed to send move display area command");
    }
    // Send the move display area command
    if (dy != -1) {
        uint8_t cmd_data[2];
        cmd_data[0] = (dy >> 8) & 0xFF; // High byte of dy
        cmd_data[1] = dy & 0xFF;        // Low byte of dy
        ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, SEEYA049_CMD_RASET, cmd_data, 2), TAG,
                            "Failed to send move display area command");
    }
    return ESP_OK;
}

#endif // SOC_MIPI_DSI_SUPPORTED

esp_err_t panel_seeya049_i2c_init(int sda_io_num, int scl_io_num) {
    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = sda_io_num,
        .scl_io_num = scl_io_num,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = I2C_MASTER_FREQ_HZ,
    };

    esp_err_t err = i2c_param_config(I2C_MASTER_NUM, &conf);
    if (err != ESP_OK) return err;

    return i2c_driver_install(I2C_MASTER_NUM, conf.mode, 0, 0, 0);
}

esp_err_t panel_seeya049_i2c_deinit(void) { return i2c_driver_delete(I2C_MASTER_NUM); }

esp_err_t panel_seeya049_i2c_write_reg(uint8_t device_addr, uint16_t reg_addr, uint16_t data) {
    uint8_t write_buf[4];
    write_buf[0] = (reg_addr >> 8) & 0xFF; // Register address high byte
    write_buf[1] = reg_addr & 0xFF;        // Register address low byte
    write_buf[2] = (data >> 8) & 0xFF;     // Data high byte
    write_buf[3] = data & 0xFF;            // Data low byte

    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (device_addr << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write(cmd, write_buf, sizeof(write_buf), true);
    i2c_master_stop(cmd);

    esp_err_t ret = i2c_master_cmd_begin(I2C_MASTER_NUM, cmd, pdMS_TO_TICKS(I2C_TIMEOUT_MS));
    i2c_cmd_link_delete(cmd);

    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to write to register 0x%04X: %d", reg_addr, ret);
    }

    return ret;
}

esp_err_t panel_seeya049_i2c_read_reg(uint8_t device_addr, uint16_t reg_addr, uint16_t *data) {
    uint8_t write_buf[2];
    uint8_t read_buf[2];
    write_buf[0] = (reg_addr >> 8) & 0xFF; // Register address high byte
    write_buf[1] = reg_addr & 0xFF;        // Register address low byte

    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    // Write register address
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (device_addr << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write(cmd, write_buf, sizeof(write_buf), true);

    // Read data
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (device_addr << 1) | I2C_MASTER_READ, true);
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

esp_err_t panel_seeya049_disp_on_off_i2c(uint8_t device_addr, bool on_off) {
    if (on_off) {
        ESP_RETURN_ON_ERROR(panel_seeya049_i2c_write_reg(device_addr, SEEYA049_CMD_DISPON, 1), TAG,
                            "Failed to write display on/off command");

        vTaskDelay(pdMS_TO_TICKS(20));
    } else {
        ESP_RETURN_ON_ERROR(panel_seeya049_i2c_write_reg(device_addr, SEEYA049_CMD_DISPOFF, 0), TAG,
                            "Failed to write display on/off command");
    }

    return ESP_OK;
}

esp_err_t panel_seeya049_move_display_area_i2c(uint8_t device_addr, uint16_t width, uint16_t height,
                                               int16_t x_offset, int16_t y_offset) {
    ESP_LOGI(TAG, "panel_seeya049_move_display_area for device 0x%02X: %d %d", device_addr,
             x_offset, y_offset);

    bool rl_sel = device_addr == SEEYA049_I2C_SECONDARY_ADDR;
    if (rl_sel) {
        x_offset = 1920 - width - x_offset;
        y_offset = 1080 - height - y_offset;
    }

    x_offset = x_offset * 2 / 3;
    // The datasheet seems to indicate this should be divided by 1.5, but experimentally
    // dividing by 2 is more correct. I'm not sure why.
    y_offset = y_offset / 2;

    // Send the move display area command
    if (x_offset != -1) {
        uint8_t cmd_data[2];
        cmd_data[0] = (x_offset >> 8) & 0xFF; // High byte of dx
        cmd_data[1] = x_offset & 0xFF;        // Low byte of dx
        ESP_RETURN_ON_ERROR(panel_seeya049_i2c_write_reg(device_addr, (SEEYA049_CMD_CASET << 8) + 0,
                                                         (uint8_t)cmd_data[0]),
                            TAG, "Failed to send move display area command");
        ESP_RETURN_ON_ERROR(panel_seeya049_i2c_write_reg(device_addr, (SEEYA049_CMD_CASET << 8) + 1,
                                                         (uint8_t)cmd_data[1]),
                            TAG, "Failed to send move display area command");
    }
    // Send the move display area command
    if (y_offset != -1) {
        uint8_t cmd_data[2];
        cmd_data[0] = (y_offset >> 8) & 0xFF; // High byte of dy
        cmd_data[1] = y_offset & 0xFF;        // Low byte of dy
        ESP_RETURN_ON_ERROR(panel_seeya049_i2c_write_reg(device_addr, (SEEYA049_CMD_RASET << 8) + 0,
                                                         (uint8_t)cmd_data[0]),
                            TAG, "Failed to send move display area command");
        ESP_RETURN_ON_ERROR(panel_seeya049_i2c_write_reg(device_addr, (SEEYA049_CMD_RASET << 8) + 1,
                                                         (uint8_t)cmd_data[1]),
                            TAG, "Failed to send move display area command");
    }
    return ESP_OK;
}

esp_err_t panel_seeya049_send_cmd_i2c(uint8_t device_addr, seeya049_lcd_init_cmd_t cmd) {
    if (cmd.cmd != 0) {
        // For each command, we need to write its data bytes to sequential registers
        for (int j = 0; j < cmd.data_bytes; j++) {
            uint16_t reg_addr;
            if (cmd.cmd > 0xFF) {
                reg_addr = cmd.cmd + j;
            } else {
                reg_addr = (cmd.cmd << 8) + j;
            }
            uint16_t data_byte = ((uint8_t *)cmd.data)[j];

            ESP_LOGI(TAG, "I2C write to %04Xh: %04Xh", reg_addr, data_byte);

            ESP_RETURN_ON_ERROR(panel_seeya049_i2c_write_reg(device_addr, reg_addr, data_byte), TAG,
                                "Failed to write command data");
        }
        if (cmd.delay_ms > 0) {
            ESP_LOGI(TAG, "Delay %dms", cmd.delay_ms);
            vTaskDelay(pdMS_TO_TICKS(cmd.delay_ms));
        }
    }
    return ESP_OK;
}

esp_err_t panel_seeya049_send_cmds_i2c(uint8_t device_addr,
                                       const seeya049_lcd_init_cmd_t *init_cmds, uint16_t size) {
    // Process each command in the sequence
    for (int i = 0; i < size; i++) {
        ESP_RETURN_ON_ERROR(panel_seeya049_send_cmd_i2c(device_addr, init_cmds[i]), TAG,
                            "Failed to send command");
    }

    return ESP_OK;
}

esp_err_t panel_seeya049_send_init_cmds_i2c(uint8_t device_addr, uint16_t width, uint16_t height,
                                            uint16_t x_offset, uint16_t y_offset) {
    ESP_LOGD(TAG, "panel_seeya049_send_init_cmds_i2c for device 0x%02X", device_addr);

    // There's some different behavior when rl_sel is high
    bool rl_sel = device_addr == SEEYA049_I2C_SECONDARY_ADDR;
    if (rl_sel) {
        x_offset = 1920 - width - x_offset;
        y_offset = 1080 - height - y_offset;
    }

    uint8_t data[6] = {0};
    for (int i = 0; i < sizeof(seeya049_init_cmds_i2c) / sizeof(seeya049_lcd_init_cmd_t); i++) {
        seeya049_lcd_init_cmd_t cmd = seeya049_init_cmds_i2c[i];

        switch (cmd.cmd) {
        // TODO: Deduplicate this code
        case SEEYA049_CMD_RESCTRL1:
            if (width != 1920 || height != 1080) {
                ESP_LOGD(TAG, "Overriding resolution to %d x %d", width, height);
                int width_div = width / 4;
                int height_div = height / 4;
                data[0] = 0x00;
                data[1] = width_div & 0xFF;
                data[2] = width_div & 0xFF;
                data[3] = height_div & 0xFF;
                data[4] = 0x00;
                if (width_div > 0xFF) {
                    data[5] |= 0x20;
                }
                if (height_div > 0xFF) {
                    data[5] |= 0x01;
                }
                cmd.data = data;
            }
            break;
        case SEEYA049_CMD_CASET:
            if (x_offset != 0) {
                ESP_LOGD(TAG, "Overriding x_offset to %d", x_offset);
                x_offset = x_offset * 2 / 3;
                data[0] = (x_offset >> 8) & 0xFF;
                data[1] = x_offset & 0xFF;
                cmd.data = &data;
            }
            break;
        case SEEYA049_CMD_RASET:
            if (y_offset != 0) {
                ESP_LOGD(TAG, "Overriding x_offset to %d", x_offset);
                // The datasheet seems to indicate this should be divided by 1.5, but
                // experimentally dividing by 2 is more correct. I'm not sure why.
                y_offset = y_offset / 2;
                data[0] = (y_offset >> 8) & 0xFF;
                data[1] = y_offset & 0xFF;
                cmd.data = &data;
            }
            break;
        case SEEYA049_CMD_MADCTL:
            // Hijack the MADCTL command to send the RL_SEL commands
            if (rl_sel) {
                ESP_LOGD(TAG, "Sending RL_SEL commands");
                ESP_RETURN_ON_ERROR(
                    panel_seeya049_send_cmds_i2c(device_addr, seeya049_init_cmds_i2c_rl_sel,
                                                 sizeof(seeya049_init_cmds_i2c_rl_sel) /
                                                     sizeof(seeya049_lcd_init_cmd_t)),
                    TAG, "Failed to send RL_SEL commands");
                continue;
            }
        }
        ESP_RETURN_ON_ERROR(panel_seeya049_send_cmd_i2c(device_addr, cmd), TAG,
                            "Failed to send command");
    }

    return ESP_OK;
}
