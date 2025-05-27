#if CONFIG_DISPLAY_SEEYA103

#include "DisplaySeeYa103.hpp"

#include <driver/gpio.h>
#include <esp_check.h>
#include <esp_lcd_panel_io.h>
#include <esp_lcd_seeya103.h>
#include <esp_lcd_seeya103_i2c.h>

static constexpr const char *TAG = "DisplaySeeYa103";

IRAM_ATTR bool DisplaySeeYa103::onColorTransDone(esp_lcd_panel_handle_t panel,
                                                 esp_lcd_dpi_panel_event_data_t *edata,
                                                 void *user_ctx) {
    auto display = static_cast<DisplaySeeYa103 *>(user_ctx);
    xSemaphoreGiveFromISR(display->updateSemaphore_, nullptr);
    return true;
}

DisplaySeeYa103::~DisplaySeeYa103() {
    if (updateSemaphore_ != nullptr) {
        vSemaphoreDelete(updateSemaphore_);
    }
    if (panelHandle_ != nullptr) {
        esp_lcd_panel_del(panelHandle_);
        panelHandle_ = nullptr;
    }
    if (mipiDbiIo_ != nullptr) {
        esp_lcd_panel_io_del(mipiDbiIo_);
        mipiDbiIo_ = nullptr;
    }
    if (mipiDsiBus_ != nullptr) {
        esp_lcd_del_dsi_bus(mipiDsiBus_);
        mipiDsiBus_ = nullptr;
    }
    if (ldoMipiPhy_ != nullptr) {
        esp_ldo_release_channel(ldoMipiPhy_);
        ldoMipiPhy_ = nullptr;
    }
};

auto DisplaySeeYa103::setup() -> bool {
    log_d("DisplaySeeYa103::setup()");
    DisplaySystem::setup();

    ppa_client_config_t ppa_client_config = {
        .oper_type = PPA_OPERATION_SRM,
        .max_pending_trans_num = 1,
        .data_burst_length = PPA_DATA_BURST_LENGTH_128,
    };
    ESP_ERROR_CHECK(ppa_register_client(&ppa_client_config, &ppaSrmClient_));

    updateSemaphore_ = xSemaphoreCreateBinary();

    // Enable debug logging for lcd.dsi.dpi
    esp_log_level_set("lcd.dsi.dpi", ESP_LOG_DEBUG);
    esp_log_level_set("seeya103", ESP_LOG_DEBUG);

    // Power pins specific to the Sol Focus board. Remove if we ever open source this.
    // 33: 3V3_ON
    // 32: 1V8_ON
    // 31: PS_ENP
    // 30: PS_PSP // Power save Positive
    // 29: PS_ENN
    // 28: PS_PSN // Power save Negative
    // Turn on pins 31, 29, 33, 32, 30, 28
    gpio_config_t config = {
        .pin_bit_mask =
            GPIO_NUM_31 | GPIO_NUM_29 | GPIO_NUM_33 | GPIO_NUM_32 | GPIO_NUM_30 | GPIO_NUM_28,
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&config);
    gpio_set_level(gpio_num_t(31), 1);
    gpio_set_level(gpio_num_t(29), 1);
    gpio_set_level(gpio_num_t(33), 1);
    gpio_set_level(gpio_num_t(32), 1);
    // Power save pins
    gpio_set_level(gpio_num_t(30), 0);
    gpio_set_level(gpio_num_t(28), 0);

    vTaskDelay(pdMS_TO_TICKS(100));

    // Turn on the power for MIPI DSI PHY, so it can go from "No Power" state to "Shutdown" state
    log_d("MIPI DSI PHY Powered on");
    esp_ldo_channel_config_t ldo_mipi_phy_config = {
        .chan_id = 3,
        .voltage_mv = 2500,
    };
    ESP_ERROR_CHECK(esp_ldo_acquire_channel(&ldo_mipi_phy_config, &ldoMipiPhy_));

    esp_err_t err = ESP_OK;

    log_d("Initialize MIPI DSI bus");
    esp_lcd_dsi_bus_config_t bus_config = panel_seeya103_dsi_bus_config();
    ESP_ERROR_CHECK(esp_lcd_new_dsi_bus(&bus_config, &mipiDsiBus_));

    log_d("Install panel IO");
    esp_lcd_dbi_io_config_t dbi_config = panel_seeya103_io_dbi_config();
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_dbi(mipiDsiBus_, &dbi_config, &mipiDbiIo_));

    log_d("Install LCD driver of seeya103");
    esp_lcd_dpi_panel_config_t dpi_config = panel_seeya103_1280_1280_dpi_config();

    seeya103_vendor_config_t vendor_config =
        panel_seeya103_vendor_config(mipiDsiBus_, &dpi_config, CONFIG_DISPLAY_RST_PIN);
    vendor_config.scaling = SEEYA103_SCALING_NONE;
    vendor_config.init_cmds = nullptr;
    vendor_config.init_cmds_size = 0;

    const esp_lcd_panel_dev_config_t panel_config = panel_seeya103_panel_config(&vendor_config);
    ESP_ERROR_CHECK(esp_lcd_new_panel_seeya103(mipiDbiIo_, &panel_config, &panelHandle_));
    ESP_ERROR_CHECK(esp_lcd_dpi_panel_get_frame_buffer(panelHandle_, 1, &fb0_));

    ESP_ERROR_CHECK(esp_lcd_panel_reset(panelHandle_));

    uint16_t x_offset = (2560 - CONFIG_DISPLAY_WIDTH) / 2;
    uint16_t y_offset = (2560 - CONFIG_DISPLAY_HEIGHT) / 2;

    err = executeI2C(true, [this, x_offset, y_offset](i2c_master_dev_handle_t device) {
        return seeya103_i2c_send_cmds(device, width(), height(), x_offset, y_offset,
                                      SEEYA103_SCALING_NONE, false, false, SEEYA103_INIT_CMDS,
                                      sizeof(SEEYA103_INIT_CMDS) / sizeof(seeya103_lcd_init_cmd_t));
    });
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "seeya103_i2c_send_cmds (left) failed: %d", err);
    }

    err = executeI2C(false, [this, x_offset, y_offset](i2c_master_dev_handle_t device) {
        return seeya103_i2c_send_cmds(device, width(), height(), x_offset, y_offset,
                                      SEEYA103_SCALING_NONE, true, true, SEEYA103_INIT_CMDS,
                                      sizeof(SEEYA103_INIT_CMDS) / sizeof(seeya103_lcd_init_cmd_t));
    });
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "seeya103_i2c_send_cmds (right) failed: %d", err);
    }

    ESP_ERROR_CHECK(esp_lcd_panel_init(panelHandle_));
    err = executeI2C(
        [](i2c_master_dev_handle_t device) { return seeya103_i2c_disp_on_off(device, true); });
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "seeya103_i2c_disp_on_off (left) failed: %d", err);
    }

    esp_lcd_dpi_panel_event_callbacks_t cbs = {
        .on_color_trans_done = onColorTransDone,
    };
    ESP_ERROR_CHECK(esp_lcd_dpi_panel_register_event_callbacks(panelHandle_, &cbs, this));

    return true;
}

void DisplaySeeYa103::updateDisplay(DisplaySystem::UpdateType updateType, bool clearFrameBuffer,
                                    int vcomNudge, DisplaySystem::UpdateType altUpdateType,
                                    ScreenSide screen, SolRect altRect) {

    assert(panelHandle_);

    int x_start = 0;
    int y_start = 0;
    int x_end = width();
    int y_end = height();
    const void *color_data = getBuffer();
    bool swapXy = true;

    // Configure PPA for 90-degree rotation
    ppa_srm_color_mode_t ppa_color_mode = PPA_SRM_COLOR_MODE_RGB888;
    ppa_srm_rotation_angle_t ppa_rotation =
        swapXy ? PPA_SRM_ROTATION_ANGLE_90 : PPA_SRM_ROTATION_ANGLE_0;

    uint32_t w = x_end - x_start;
    uint32_t h = y_end - y_start;
    uint32_t input_pic_w = width();
    uint32_t input_pic_h = height();
    int rotated_x_start = x_start;
    int rotated_y_start = y_start;
    int rotated_x_end = x_end;
    int rotated_y_end = y_end;

    if (swapXy) {
        input_pic_w = height();
        input_pic_h = width();
        rotated_x_start = y_start;
        rotated_y_start = width() - x_end;
        rotated_x_end = y_end;
        rotated_y_end = width() - x_start;
    }

    // TODO: If we wanted to speed this up we could make it fully async
    ppa_srm_oper_config_t srm_config = {};
    srm_config.in.buffer = color_data;
    srm_config.in.pic_w = input_pic_w;
    srm_config.in.pic_h = input_pic_h;
    srm_config.in.block_w = (uint32_t)w;
    srm_config.in.block_h = (uint32_t)h;
    srm_config.in.block_offset_x = x_start;
    srm_config.in.block_offset_y = y_start;
    srm_config.in.srm_cm = ppa_color_mode;

    srm_config.out.buffer = fb0_;
    srm_config.out.buffer_size = width() * height() * 3;
    srm_config.out.pic_w = (uint32_t)width();
    srm_config.out.pic_h = (uint32_t)height();
    srm_config.out.block_offset_x = rotated_x_start;
    srm_config.out.block_offset_y = rotated_y_start;
    srm_config.out.srm_cm = ppa_color_mode;

    srm_config.rotation_angle = ppa_rotation;
    srm_config.scale_x = 1.0f;
    srm_config.scale_y = 1.0f;
    srm_config.mirror_x = false;
    srm_config.mirror_y = false;
    srm_config.rgb_swap = false;
    srm_config.byte_swap = false;
    srm_config.alpha_update_mode = PPA_ALPHA_NO_CHANGE;
    srm_config.mode = PPA_TRANS_MODE_BLOCKING;
    srm_config.user_data = nullptr;

    ESP_ERROR_CHECK(ppa_do_scale_rotate_mirror(ppaSrmClient_, &srm_config));

    esp_lcd_panel_draw_bitmap(panelHandle_, rotated_x_start, rotated_y_start, rotated_x_end,
                              rotated_y_end, fb0_);

    // Wait until the color transfer is done (so we don't overwrite the buffer while it's being
    // drawn). If we need more performance we could also do double buffering here so we could start
    // drawing the next frame while the current one is getting copied.
    if (xSemaphoreTake(updateSemaphore_, portMAX_DELAY) != pdPASS) {
        log_e("Failed to take display update");
        return;
    }

    if (clearFrameBuffer) {
        fillScreen(bg);
    }
}

auto DisplaySeeYa103::executeI2C(bool left,
                                 std::function<esp_err_t(i2c_master_dev_handle_t)> command)
    -> esp_err_t {
    i2c_master_bus_handle_t bus = nullptr;
    i2c_master_dev_handle_t device = nullptr;
    // Initialize I2C buses for both screens
    i2c_master_bus_config_t bus_config = {
        .i2c_port = I2C_NUM_0,
        .sda_io_num = static_cast<gpio_num_t>(left ? sdaLGpio_ : sdaRGpio_),
        .scl_io_num = static_cast<gpio_num_t>(left ? sclLGpio_ : sclRGpio_),
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .intr_priority = 0,
        .trans_queue_depth = 0,
        .flags =
            {
                .enable_internal_pullup = true,
                .allow_pd = false,
            },
    };
    ESP_RETURN_ON_ERROR(i2c_new_master_bus(&bus_config, &bus), TAG, "Failed to create I2C bus");
    ESP_RETURN_ON_ERROR(seeya103_i2c_init(bus, &device), TAG, "Failed to initialize I2C device");
    ESP_RETURN_ON_ERROR(command(device), TAG, "Failed to execute I2C command");
    ESP_RETURN_ON_ERROR(i2c_master_bus_rm_device(device), TAG, "Failed to remove I2C device");
    ESP_RETURN_ON_ERROR(i2c_del_master_bus(bus), TAG, "Failed to delete I2C bus");

    return ESP_OK;
}

auto DisplaySeeYa103::executeI2C(std::function<esp_err_t(i2c_master_dev_handle_t)> command)
    -> esp_err_t {
    esp_err_t err = executeI2C(true, command);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "seeya103_i2c_write_reg (left) failed: %d", err);
    }
    err = executeI2C(false, command);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "seeya103_i2c_write_reg (right) failed: %d", err);
    }
    return err;
}

#endif