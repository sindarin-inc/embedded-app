/*
 * Driver for SeeYA SY103WAM01 (1.03" micro-OLED, 2560x2560) via MIPI DSI
 */

#include "esp_lcd_seeya103.h"

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
    seeya103_scaling_t scaling;
    const seeya103_lcd_init_cmd_t *init_cmds;
    uint16_t init_cmds_size;
    struct {
        unsigned int reset_level : 1;
    } flags;
    // To save the original functions of MIPI DPI panel
    esp_err_t (*del)(esp_lcd_panel_t *panel);
    esp_err_t (*init)(esp_lcd_panel_t *panel);
} seeya103_panel_t;

static const char *TAG = "seeya103";

esp_err_t panel_seeya103_send_init_cmds_with_fn(const seeya103_lcd_init_cmd_t *init_cmds,
                                                uint16_t init_cmds_size, uint16_t width,
                                                uint16_t height, uint16_t x_offset,
                                                uint16_t y_offset, seeya103_scaling_t scaling,
                                                seeya103_send_cmd_fn_t send_cmd_fn,
                                                void *user_data) {
    if (init_cmds == NULL || init_cmds_size == 0) {
        ESP_LOGW(TAG, "No init commands provided");
        return ESP_OK;
    }

    uint8_t data[] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

    int display_width = width;
    int display_height = height;

    switch (scaling) {
    case SEEYA103_SCALING_1_33X:
        // Multiply by 4/3 then divide by 8. Round up.
        display_width = display_width / 6;
        display_height = display_height / 6;
        break;
    case SEEYA103_SCALING_2X:
        // Multiply by 2 then divide by 8
        display_width = display_width / 4;
        display_height = display_height / 4;
        break;
    case SEEYA103_SCALING_NONE:
    default:
        display_width = display_width / 8;
        display_height = display_height / 8;
        break;
    }

    for (int i = 0; i < init_cmds_size; i++) {
        seeya103_lcd_init_cmd_t cmd = init_cmds[i];

        if (cmd.cmd != 0) {
            switch (cmd.cmd) {

            // Override the width and height if we're using a non-standard resolution
            case SEEYA103_CMD_RESCTRL1:
                if (width != SEEYA103_HRES_DEFAULT || height != SEEYA103_VRES_DEFAULT ||
                    scaling != SEEYA103_SCALING_DEFAULT) {

                    data[0] = ((uint8_t *)cmd.data)[0];
                    data[1] = display_width & 0xFF;
                    data[2] = display_height & 0xFF;
                    data[3] = 0x00;

                    if (display_width > 0xFF) {
                        data[3] |= 0x10;
                    }
                    if (display_height > 0xFF) {
                        data[3] |= 0x01;
                    }
                    ESP_LOGD(TAG,
                             "Overriding resolution to: input_width=%d, input_height=%d, "
                             "display_width=%d, display_height=%d",
                             width, height, display_width * 8, display_height * 8);

                    cmd.data = &data;
                }
                break;

            case SEEYA103_CMD_RESCTRL2:
            case SEEYA103_CMD_RESCTRL3:
                // For some reason, 1280x1280 not scaled mode seems to needs the "OSC divisor for
                // data path" to be set to 0x01.
                // TODO: There's probably a more generic way to handle this.
                if (display_width * 8 <= 1280 && display_width * 8 <= 1280) {
                    ESP_LOGD(TAG, "Overriding \"OSC divisor for data path\" to 0x01");
                    data[0] = ((uint8_t *)cmd.data)[0];
                    data[1] = ((uint8_t *)cmd.data)[1];
                    data[2] = ((uint8_t *)cmd.data)[2];
                    data[3] = ((uint8_t *)cmd.data)[3];
                    data[4] = ((uint8_t *)cmd.data)[4];
                    data[5] = ((uint8_t *)cmd.data)[5];
                    data[6] = 0x01;
                    cmd.data = &data;
                }
                break;
            case SEEYA103_CMD_CASET:
                // if (x_offset != 0) {
                ESP_LOGD(TAG, "Overriding x_offset to %d", x_offset);
                data[0] = (x_offset >> 8) & 0xFF;
                data[1] = x_offset & 0xFF;
                cmd.data = &data;
                // }
                break;
            case SEEYA103_CMD_RASET:
                // if (y_offset != 0) {
                ESP_LOGD(TAG, "Overriding y_offset to %d", y_offset);
                data[0] = (y_offset >> 8) & 0xFF;
                data[1] = y_offset & 0xFF;
                cmd.data = &data;
                // }
                break;
            case SEEYA103_CMD_SCACTRL:
                if (scaling != SEEYA103_SCALING_DEFAULT) {
                    ESP_LOGD(TAG, "Setting scaling to %d", scaling);
                    data[0] = (uint8_t)scaling;
                    cmd.data = &data;
                }
                break;
            default:
                break;
            }
            ESP_RETURN_ON_ERROR(send_cmd_fn(cmd, user_data), TAG, "send command failed");
        }
        if (cmd.delay_ms > 0) {
            ESP_LOGI(TAG, "Delay %dms", cmd.delay_ms);
            vTaskDelay(pdMS_TO_TICKS(cmd.delay_ms));
        }
    }

    return ESP_OK;
}

#if SOC_MIPI_DSI_SUPPORTED

// Forward declaration of panel methods
static esp_err_t panel_seeya103_del(esp_lcd_panel_t *panel);
static esp_err_t panel_seeya103_init(esp_lcd_panel_t *panel);
static esp_err_t panel_seeya103_reset(esp_lcd_panel_t *panel);
static esp_err_t panel_seeya103_mirror(esp_lcd_panel_t *panel, bool mirror_x, bool mirror_y);
static esp_err_t panel_seeya103_disp_on_off(esp_lcd_panel_t *panel, bool on_off);
static esp_err_t panel_seeya103_sleep(esp_lcd_panel_t *panel, bool sleep);

esp_err_t esp_lcd_new_panel_seeya103(const esp_lcd_panel_io_handle_t io,
                                     const esp_lcd_panel_dev_config_t *panel_dev_config,
                                     esp_lcd_panel_handle_t *ret_panel) {
    ESP_RETURN_ON_FALSE(io && panel_dev_config && ret_panel, ESP_ERR_INVALID_ARG, TAG,
                        "invalid arguments");
    seeya103_vendor_config_t *vendor_config =
        (seeya103_vendor_config_t *)panel_dev_config->vendor_config;
    ESP_RETURN_ON_FALSE(vendor_config && vendor_config->mipi_config.dpi_config &&
                            vendor_config->mipi_config.dsi_bus,
                        ESP_ERR_INVALID_ARG, TAG, "invalid vendor config");

    esp_err_t ret = ESP_OK;
    seeya103_panel_t *seeya103 = (seeya103_panel_t *)calloc(1, sizeof(seeya103_panel_t));
    ESP_RETURN_ON_FALSE(seeya103, ESP_ERR_NO_MEM, TAG, "no mem for seeya103 panel");

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
    seeya103->io = io;
    seeya103->init_cmds = vendor_config->init_cmds;
    seeya103->init_cmds_size = vendor_config->init_cmds_size;
    seeya103->reset_gpio_num = panel_dev_config->reset_gpio_num;
    seeya103->width = dpi_config.video_timing.h_size;
    seeya103->height = dpi_config.video_timing.v_size;
    seeya103->x_offset = vendor_config->x_offset;
    seeya103->y_offset = vendor_config->y_offset;
    seeya103->scaling = vendor_config->scaling;
    seeya103->flags.reset_level = panel_dev_config->flags.reset_active_high;

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
        uint32_t max_bandwidth = vendor_config->mipi_config.lane_num * 1000000000;
        max_bandwidth = max_bandwidth - max_bandwidth * 0.04;
        if (ideal_dpi_clock_hz >= 80000000) {
            // Above 80 MHz, the ESP32-P4 can't keep up with the refresh rate
            dpi_config.dpi_clock_freq_mhz = 80;
            ESP_LOGD(TAG,
                     "Limiting DPI clock to %" PRIu32 " MHz for reduced refresh rate of %" PRIu16
                     " Hz (ideal: %.2f MHz MIPI bandwidth: %" PRIu32 " mbps)",
                     dpi_config.dpi_clock_freq_mhz, vendor_config->refresh_rate,
                     ideal_dpi_clock_hz / 1000000.0, dpi_config.dpi_clock_freq_mhz * 24);

        } else if (ideal_dpi_clock_hz <= 40000000) {
            // Below 40 MHz, the display seems to get weird
            // TODO: Revisit this
            dpi_config.dpi_clock_freq_mhz = 40;
            ESP_LOGD(TAG,
                     "Limiting DPI clock to %" PRIu32 " MHz for reduced refresh rate of %" PRIu16
                     " Hz (ideal: %.2f MHz MIPI bandwidth: %" PRIu32 " mbps)",
                     dpi_config.dpi_clock_freq_mhz, vendor_config->refresh_rate,
                     ideal_dpi_clock_hz / 1000000.0, dpi_config.dpi_clock_freq_mhz * 24);

        } else {
            dpi_config.dpi_clock_freq_mhz = (ideal_dpi_clock_hz + 999999) / 1000000;
            ESP_LOGD(TAG,
                     "Overriding DPI clock to %" PRIu32 " MHz for refresh rate %" PRIu16
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
    seeya103->del = (*ret_panel)->del;
    seeya103->init = (*ret_panel)->init;
    // Overwrite the functions of MIPI DPI panel
    (*ret_panel)->del = panel_seeya103_del;
    (*ret_panel)->init = panel_seeya103_init;
    (*ret_panel)->reset = panel_seeya103_reset;
    (*ret_panel)->mirror = panel_seeya103_mirror;
    (*ret_panel)->disp_on_off = panel_seeya103_disp_on_off;
    (*ret_panel)->disp_sleep = panel_seeya103_sleep;
    (*ret_panel)->user_data = seeya103;
    ESP_LOGD(TAG, "new seeya103 panel @%p", seeya103);

    // Log porch settings
    ESP_LOGD(TAG,
             "width: %d x_offset: %" PRIu16 " hsync_pulse_width: %" PRIu32
             " hsync_back_porch: %" PRIu32 " hsync_front_porch: %" PRIu32,
             seeya103->width, seeya103->x_offset, dpi_config.video_timing.hsync_pulse_width,
             dpi_config.video_timing.hsync_back_porch, dpi_config.video_timing.hsync_front_porch);
    ESP_LOGD(TAG,
             "v_size: %d y_offset: %" PRIu16 " vsync_pulse_width: %" PRIu32
             " vsync_back_porch: %" PRIu32 " vsync_front_porch: %" PRIu32,
             seeya103->height, seeya103->y_offset, dpi_config.video_timing.vsync_pulse_width,
             dpi_config.video_timing.vsync_back_porch, dpi_config.video_timing.vsync_front_porch);

    return ESP_OK;

err:
    if (seeya103) {
        if (panel_dev_config->reset_gpio_num >= 0) {
            gpio_reset_pin(panel_dev_config->reset_gpio_num);
        }
        free(seeya103);
    }
    return ret;
}

static esp_err_t panel_seeya103_del(esp_lcd_panel_t *panel) {
    ESP_LOGI(TAG, "panel_seeya103_del");
    seeya103_panel_t *seeya103 = (seeya103_panel_t *)panel->user_data;

    if (seeya103->reset_gpio_num >= 0) {
        // Reset display
        ESP_RETURN_ON_ERROR(panel_seeya103_reset(panel), TAG, "reset display failed");
        gpio_reset_pin(seeya103->reset_gpio_num);
    }
    // Delete MIPI DPI panel
    seeya103->del(panel);
    ESP_LOGI(TAG, "del seeya103 panel @%p", seeya103);
    free(seeya103);

    return ESP_OK;
}

// Helper function for MIPI interface
static esp_err_t panel_seeya103_send_cmd_mipi(seeya103_lcd_init_cmd_t cmd, void *user_data) {
    seeya103_panel_t *seeya103 = (seeya103_panel_t *)user_data;
    esp_lcd_panel_io_handle_t io = seeya103->io;

    char data_str[64] = "";
    if (cmd.data && cmd.data_bytes > 0) {
        strcat(data_str, ": ");
        for (int i = 0; i < cmd.data_bytes && i < 16; i++) {
            sprintf(data_str + strlen(data_str), "0x%02X ", ((uint8_t *)cmd.data)[i]);
        }
    }
    ESP_LOGI(TAG, "MIPI TX %02Xh%s", cmd.cmd, data_str);

    return esp_lcd_panel_io_tx_param(io, cmd.cmd, cmd.data, cmd.data_bytes);
}

// Wrapper function for MIPI interface
esp_err_t panel_seeya103_send_init_cmds(esp_lcd_panel_t *panel) {
    seeya103_panel_t *seeya103 = (seeya103_panel_t *)panel->user_data;

    return panel_seeya103_send_init_cmds_with_fn(
        seeya103->init_cmds, seeya103->init_cmds_size, seeya103->width, seeya103->height,
        seeya103->x_offset, seeya103->y_offset, seeya103->scaling, panel_seeya103_send_cmd_mipi,
        seeya103);
}

static esp_err_t panel_seeya103_init(esp_lcd_panel_t *panel) {
    ESP_LOGI(TAG, "panel_seeya103_init");
    seeya103_panel_t *seeya103 = (seeya103_panel_t *)panel->user_data;
    ESP_RETURN_ON_ERROR(panel_seeya103_send_init_cmds(panel), TAG, "send init commands failed");

    // What happens in here? See:
    // https://github.com/espressif/esp-idf/blob/0d6099ec533c4b647fb7a7b0b8942bc7aeb82f90/components/esp_lcd/dsi/esp_lcd_panel_dpi.c#L403
    ESP_RETURN_ON_ERROR(seeya103->init(panel), TAG, "init MIPI DPI panel failed");

    return ESP_OK;
}

static esp_err_t panel_seeya103_reset(esp_lcd_panel_t *panel) {
    ESP_LOGI(TAG, "panel_seeya103_reset");
    seeya103_panel_t *seeya103 = (seeya103_panel_t *)panel->user_data;
    esp_lcd_panel_io_handle_t io = seeya103->io;

    // Perform hardware reset
    if (seeya103->reset_gpio_num >= 0) {
        ESP_LOGD(TAG, "Reset pin %d to %d", seeya103->reset_gpio_num, seeya103->flags.reset_level);
        gpio_set_level(seeya103->reset_gpio_num, seeya103->flags.reset_level);
        vTaskDelay(pdMS_TO_TICKS(1));
        ESP_LOGD(TAG, "Reset pin %d to %d", seeya103->reset_gpio_num, !seeya103->flags.reset_level);
        gpio_set_level(seeya103->reset_gpio_num, !seeya103->flags.reset_level);
        vTaskDelay(pdMS_TO_TICKS(120));
    } else if (io) { // Perform software reset
        ESP_LOGI(TAG, "MIPI TX: %02Xh", SEEYA103_CMD_SWRESET);
        ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, SEEYA103_CMD_SWRESET, NULL, 0), TAG,
                            "send command failed");
        vTaskDelay(pdMS_TO_TICKS(120));
    }

    return ESP_OK;
}

static esp_err_t panel_seeya103_mirror(esp_lcd_panel_t *panel, bool mirror_x, bool mirror_y) {
    seeya103_panel_t *seeya103 = (seeya103_panel_t *)panel->user_data;
    esp_lcd_panel_io_handle_t io = seeya103->io;

    uint8_t madctl_val = 0;

    if (mirror_x) {
        madctl_val |= SEEYA103_CMD_MADCTL_MX;
    }
    // NOTE: Mirroring the Y axis seems to mess with the pixel format so colors get all weird
    // Red -> Yellow, Blue -> Cyan, Green -> Magenta
    // These are inverted colors, but simply sending the inverse of the colors doesn't seem to
    // work.
    if (mirror_y) {
        madctl_val |= SEEYA103_CMD_MADCTL_MY;
    }

    ESP_LOGI(TAG, "MIPI TX: %02Xh data: 0x%02X", SEEYA103_CMD_MADCTL, madctl_val);
    ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, SEEYA103_CMD_MADCTL, &madctl_val, 1), TAG,
                        "send command failed");

    return ESP_OK;
}

static esp_err_t panel_seeya103_disp_on_off(esp_lcd_panel_t *panel, bool on_off) {
    ESP_LOGI(TAG, "panel_seeya103_disp_on_off %d", on_off);

    seeya103_panel_t *seeya103 = (seeya103_panel_t *)panel->user_data;
    esp_lcd_panel_io_handle_t io = seeya103->io;

    if (on_off) {
        ESP_LOGI(TAG, "MIPI TX: %02Xh", SEEYA103_CMD_DISPON);
        ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, SEEYA103_CMD_DISPON, NULL, 0), TAG,
                            "send command failed");
        ESP_LOGI(TAG, "Delay 20ms");
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
        ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, SEEYA103_CMD_DISPOFF, NULL, 0), TAG,
                            "send command failed");
    }
    return ESP_OK;
}

static esp_err_t panel_seeya103_sleep(esp_lcd_panel_t *panel, bool sleep) {
    ESP_LOGI(TAG, "panel_seeya103_sleep");

    seeya103_panel_t *seeya103 = (seeya103_panel_t *)panel->user_data;
    esp_lcd_panel_io_handle_t io = seeya103->io;

    if (sleep) {
        ESP_LOGI(TAG, "MIPI TX: 0x10");
        ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, SEEYA103_CMD_SLPIN, NULL, 0), TAG,
                            "send command failed");
    } else {
        ESP_LOGI(TAG, "MIPI TX: 0x11");
        ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, SEEYA103_CMD_SLPOUT, NULL, 0), TAG,
                            "send command failed");
    }
    vTaskDelay(pdMS_TO_TICKS(100));

    return ESP_OK;
}

esp_err_t panel_seeya103_all_pixels_on_off(esp_lcd_panel_t *panel, bool on_off) {
    ESP_LOGI(TAG, "panel_seeya103_all_pixels_on_off %d", on_off);

    seeya103_panel_t *seeya103 = (seeya103_panel_t *)panel->user_data;
    esp_lcd_panel_io_handle_t io = seeya103->io;

    if (on_off) {
        ESP_LOGI(TAG, "MIPI TX: 0x23");
        ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, SEEYA103_CMD_ALLPON, NULL, 0), TAG,
                            "send command failed");
    } else {
        ESP_LOGI(TAG, "MIPI TX: 0x24");
        ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, SEEYA103_CMD_ALLPOFF, NULL, 0), TAG,
                            "send command failed");
    }

    return ESP_OK;
}

esp_err_t panel_seeya103_run_selftest(esp_lcd_panel_t *panel) {
    ESP_RETURN_ON_FALSE(panel, ESP_ERR_INVALID_ARG, TAG, "parameter is invalid");

    ESP_LOGI(TAG, "panel_seeya103_run_selftest");
    seeya103_panel_t *seeya103 = (seeya103_panel_t *)panel->user_data;
    seeya103->init_cmds = seeya103_init_cmds_selftest;
    seeya103->init_cmds_size =
        sizeof(seeya103_init_cmds_selftest) / sizeof(seeya103_lcd_init_cmd_t);

    // First reset the panel to ensure we're in a known state
    ESP_RETURN_ON_ERROR(panel_seeya103_reset(panel), TAG, "reset display failed");

    panel_seeya103_send_init_cmds(panel);

    return ESP_OK;
}

esp_err_t panel_seeya103_move_display_area(esp_lcd_panel_t *panel, int16_t x_offset,
                                           int16_t y_offset) {
    ESP_LOGI(TAG, "panel_seeya103_move_display_area %d %d", x_offset, y_offset);

    seeya103_panel_t *seeya103 = (seeya103_panel_t *)panel->user_data;
    esp_lcd_panel_io_handle_t io = seeya103->io;

    // Send the move display area command
    if (x_offset != -1) {
        uint8_t cmd_data[2];
        cmd_data[0] = (x_offset >> 8) & 0xFF; // High byte of dx
        cmd_data[1] = x_offset & 0xFF;        // Low byte of dx
        ESP_LOGI(TAG, "MIPI TX: 0x2A data: 0x%02X 0x%02X", cmd_data[0], cmd_data[1]);
        ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, SEEYA103_CMD_CASET, cmd_data, 2), TAG,
                            "Failed to send move display area command");
    }
    // Send the move display area command
    if (y_offset != -1) {
        uint8_t cmd_data[2];
        cmd_data[0] = (y_offset >> 8) & 0xFF; // High byte of dy
        cmd_data[1] = y_offset & 0xFF;        // Low byte of dy
        ESP_LOGI(TAG, "MIPI TX: 0x2B data: 0x%02X 0x%02X", cmd_data[0], cmd_data[1]);
        ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, SEEYA103_CMD_RASET, cmd_data, 2), TAG,
                            "Failed to send move display area command");
    }
    return ESP_OK;
}

#endif // SOC_MIPI_DSI_SUPPORTED

esp_err_t panel_seeya103_i2c_init(int sda_io_num, int scl_io_num) {
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

esp_err_t panel_seeya103_i2c_deinit(void) { return i2c_driver_delete(I2C_MASTER_NUM); }

esp_err_t panel_seeya103_i2c_write_reg(uint16_t reg_addr, uint16_t data) {
    uint8_t write_buf[4];
    write_buf[0] = (reg_addr >> 8) & 0xFF; // Register address high byte
    write_buf[1] = reg_addr & 0xFF;        // Register address low byte
    write_buf[2] = (data >> 8) & 0xFF;     // Data high byte
    write_buf[3] = data & 0xFF;            // Data low byte

    ESP_LOGI(TAG, "I2C TX %04Xh: %04Xh", reg_addr, data);

    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (SEEYA103_I2C_ADDR << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write(cmd, write_buf, sizeof(write_buf), true);
    i2c_master_stop(cmd);

    esp_err_t ret = i2c_master_cmd_begin(I2C_MASTER_NUM, cmd, pdMS_TO_TICKS(I2C_TIMEOUT_MS));
    i2c_cmd_link_delete(cmd);

    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to write to register 0x%04X: %d", reg_addr, ret);
    }

    return ret;
}

esp_err_t panel_seeya103_i2c_read_reg(uint16_t reg_addr, uint16_t *data) {
    uint8_t write_buf[2];
    uint8_t read_buf[2];
    write_buf[0] = (reg_addr >> 8) & 0xFF; // Register address high byte
    write_buf[1] = reg_addr & 0xFF;        // Register address low byte

    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    // Write register address
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (SEEYA103_I2C_ADDR << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write(cmd, write_buf, sizeof(write_buf), true);

    // Read data
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (SEEYA103_I2C_ADDR << 1) | I2C_MASTER_READ, true);
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

esp_err_t panel_seeya103_disp_on_off_i2c(bool on_off) {
    ESP_LOGI(TAG, "panel_seeya103_disp_on_off_i2c %d", on_off);
    if (on_off) {
        ESP_RETURN_ON_ERROR(panel_seeya103_i2c_write_reg(SEEYA103_CMD_DISPON << 8, 0), TAG,
                            "Failed to write display on/off command");
        ESP_LOGI(TAG, "Delay 20ms");
        vTaskDelay(pdMS_TO_TICKS(20));
        ESP_RETURN_ON_ERROR(panel_seeya103_i2c_write_reg(0xF000, 0xAA), TAG,
                            "Failed to write display on/off command");
        ESP_RETURN_ON_ERROR(panel_seeya103_i2c_write_reg(0xF001, 0x11), TAG,
                            "Failed to write display on/off command");
        ESP_RETURN_ON_ERROR(panel_seeya103_i2c_write_reg(0xC000, 0xFF), TAG,
                            "Failed to write display on/off command");
    } else {
        ESP_RETURN_ON_ERROR(panel_seeya103_i2c_write_reg(SEEYA103_CMD_DISPOFF, 0), TAG,
                            "Failed to write display on/off command");
    }

    return ESP_OK;
}

esp_err_t panel_seeya103_move_display_area_i2c(int16_t x_offset, int16_t y_offset) {
    ESP_LOGI(TAG, "panel_seeya103_move_display_area_i2c for device 0x%02X: %d %d",
             SEEYA103_I2C_ADDR, x_offset, y_offset);

    // Send the move display area command
    if (x_offset != -1) {
        uint8_t cmd_data[2];
        cmd_data[0] = (x_offset >> 8) & 0xFF; // High byte of dx
        cmd_data[1] = x_offset & 0xFF;        // Low byte of dx
        ESP_RETURN_ON_ERROR(
            panel_seeya103_i2c_write_reg((SEEYA103_CMD_CASET << 8) + 0, (uint8_t)cmd_data[0]), TAG,
            "Failed to send move display area command");
        ESP_RETURN_ON_ERROR(
            panel_seeya103_i2c_write_reg((SEEYA103_CMD_CASET << 8) + 1, (uint8_t)cmd_data[1]), TAG,
            "Failed to send move display area command");
    }
    // Send the move display area command
    if (y_offset != -1) {
        uint8_t cmd_data[2];
        cmd_data[0] = (y_offset >> 8) & 0xFF; // High byte of dy
        cmd_data[1] = y_offset & 0xFF;        // Low byte of dy
        ESP_RETURN_ON_ERROR(
            panel_seeya103_i2c_write_reg((SEEYA103_CMD_RASET << 8) + 0, (uint8_t)cmd_data[0]), TAG,
            "Failed to send move display area command");
        ESP_RETURN_ON_ERROR(
            panel_seeya103_i2c_write_reg((SEEYA103_CMD_RASET << 8) + 1, (uint8_t)cmd_data[1]), TAG,
            "Failed to send move display area command");
    }
    return ESP_OK;
}

// Helper function for I2C interface
static esp_err_t panel_seeya103_i2c_send_cmd(seeya103_lcd_init_cmd_t cmd, void *user_data) {
    static int data_pointer = 0;
    // MIPI commands use this special register to offset the data pointer on the following command
    if (cmd.cmd == SEEYA103_CMD_POINTER) {
        data_pointer = *(int *)cmd.data;
        return ESP_OK;
    }

    // For each command, we need to write its data bytes to sequential registers
    uint16_t reg_addr;
    if (cmd.cmd > 0xFF) {
        reg_addr = cmd.cmd;
    } else {
        reg_addr = (cmd.cmd << 8);
    }
    if (data_pointer > 0) {
        reg_addr += data_pointer;
    }
    if (cmd.data_bytes > 0) {
        for (int j = 0; j < cmd.data_bytes; j++) {
            uint16_t data_byte = ((uint8_t *)cmd.data)[j];
            ESP_RETURN_ON_ERROR(panel_seeya103_i2c_write_reg(reg_addr, data_byte), TAG,
                                "Failed to write command data");
            reg_addr += 1;
        }
    } else {
        ESP_RETURN_ON_ERROR(panel_seeya103_i2c_write_reg(reg_addr, 0), TAG,
                            "Failed to write command");
    }
    data_pointer = 0;
    return ESP_OK;
}

esp_err_t panel_seeya103_send_cmds_i2c(uint16_t width, uint16_t height, uint16_t x_offset,
                                       uint16_t y_offset, seeya103_scaling_t scaling,
                                       const seeya103_lcd_init_cmd_t *init_cmds,
                                       uint16_t init_cmds_size) {
    ESP_LOGD(TAG, "panel_seeya103_send_cmds_i2c for device 0x%02X", SEEYA103_I2C_ADDR);

    return panel_seeya103_send_init_cmds_with_fn(init_cmds, init_cmds_size, width, height, x_offset,
                                                 y_offset, scaling, panel_seeya103_i2c_send_cmd,
                                                 NULL);
}
