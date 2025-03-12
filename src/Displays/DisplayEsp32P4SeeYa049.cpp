#if CONFIG_DISPLAY_ESP32_P4_SEEYA_049

#include "DisplayEsp32P4SeeYa049.hpp"

#include <driver/gpio.h>
#include <esp_check.h>
#include <esp_lcd_seeya049.h>

IRAM_ATTR bool DisplayEsp32P4SeeYa049::onColorTransDone(esp_lcd_panel_handle_t panel,
                                                        esp_lcd_dpi_panel_event_data_t *edata,
                                                        void *user_ctx) {
    auto display = static_cast<DisplayEsp32P4SeeYa049 *>(user_ctx);
    // TODO: This isn't quite right either since it causes the screen to flash briefly
    if (display->clearFrameBuffer_) {
        display->fillScreen(display->bg);
    }
    xSemaphoreGiveFromISR(display->updateSemaphore_, nullptr);
    return true;
}

auto DisplayEsp32P4SeeYa049::setup() -> bool {
    DisplaySystem::setup();
    invertDisplay(true);

    updateSemaphore_ = xSemaphoreCreateBinary();

    // Turn on the power for MIPI DSI PHY, so it can go from "No Power" state to "Shutdown" state
    log_i("MIPI DSI PHY Powered on");
    esp_ldo_channel_config_t ldoChannelConfig = {
        .chan_id = 3,
        .voltage_mv = 2500,
        .flags =
            {
                .adjustable = false,
                .owned_by_hw = false,
                .bypass = false,
            },
    };
    auto err = esp_ldo_acquire_channel(&ldoChannelConfig, &ldoMipiPhy_);
    if (err != ESP_OK) {
        log_e("Failed to acquire MIPI DSI PHY channel");
        return false;
    }

    log_i("Initialize MIPI DSI bus");
    esp_lcd_dsi_bus_config_t dsiBusConfig = {
        .bus_id = 0,
        .num_data_lanes = 2,
        .phy_clk_src = MIPI_DSI_PHY_CLK_SRC_DEFAULT,
        .lane_bit_rate_mbps = 1000,
    };

    err = esp_lcd_new_dsi_bus(&dsiBusConfig, &mipiDsiBus_);
    if (err != ESP_OK) {
        log_e("Failed to create MIPI DSI bus");
        return false;
    }

    log_i("Install panel IO");
    esp_lcd_dbi_io_config_t dbiIoConfig = {
        .virtual_channel = 0,
        .lcd_cmd_bits = 8,
        .lcd_param_bits = 8,
    };
    err = esp_lcd_new_panel_io_dbi(mipiDsiBus_, &dbiIoConfig, &mipiDbiIo_);
    if (err != ESP_OK) {
        log_e("Failed to create MIPI DSI panel IO");
        return false;
    }

    // log_i("Install LCD driver of seeya049");
    // esp_lcd_dpi_panel_config_t dpiPanelConfig = {
    //     .virtual_channel = 0,
    //     .dpi_clk_src = MIPI_DSI_DPI_CLK_SRC_DEFAULT,
    //     .dpi_clock_freq_mhz = 80, // TODO: Check if this is correct
    //     .pixel_format = LCD_COLOR_PIXEL_FORMAT_RGB888,
    //     .in_color_format = LCD_COLOR_FMT_RGB888,
    //     .out_color_format = LCD_COLOR_FMT_RGB888,
    //     .num_fbs = 1,
    //     .video_timing =
    //         {
    //             .h_size = CONFIG_DISPLAY_WIDTH,
    //             .v_size = CONFIG_DISPLAY_HEIGHT,
    //             .hsync_pulse_width = 10, // Adjusted from datasheet
    //             .hsync_back_porch = 20,
    //             .hsync_front_porch = 20,
    //             .vsync_pulse_width = 4,
    //             .vsync_back_porch = 16,
    //             .vsync_front_porch = 16,
    //         },
    //     .flags =
    //         {
    //             .use_dma2d = true,
    //             .disable_lp = false,
    //         },
    // };
    // seeya049_vendor_config_t vendorConfig = {
    //     .init_cmds = nullptr,
    //     .init_cmds_size = 0,
    //     .mipi_config =
    //         {
    //             .dsi_bus = mipiDsiBus_,
    //             .dpi_config = &dpiPanelConfig,
    //             .lane_num = 2,
    //         },
    // };
    const esp_lcd_panel_dev_config_t panelDevConfig = {
        .reset_gpio_num = GPIO_NUM_NC,
        .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB,
        .data_endian = LCD_RGB_DATA_ENDIAN_BIG,
        .bits_per_pixel = 16,
        .flags =
            {
                .reset_active_high = false,
            },
        // .vendor_config = &vendorConfig,
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_seeya049(mipiDbiIo_, &panelDevConfig, &panelHandle_));
    ESP_ERROR_CHECK(esp_lcd_panel_reset(panelHandle_));
    ESP_ERROR_CHECK(esp_lcd_panel_init(panelHandle_));
    ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(panelHandle_, true));
    log_i("DisplayEsp32P4SeeYa049::setup() done with size %d x %d", nativeWidth(), nativeHeight());

    esp_lcd_dpi_panel_event_callbacks_t cbs = {
        .on_color_trans_done = onColorTransDone,
    };
    ESP_ERROR_CHECK(esp_lcd_dpi_panel_register_event_callbacks(panelHandle_, &cbs, this));

    xSemaphoreGive(updateSemaphore_);

    return true;
}

void DisplayEsp32P4SeeYa049::updateDisplay(DisplaySystem::UpdateType updateType,
                                           bool clearFrameBuffer, int vcomNudge,
                                           DisplaySystem::UpdateType altUpdateType,
                                           ScreenSide screen, SolRect altRect) {

    // If we're not done rendering from last time yet, wait
    if (xSemaphoreTake(updateSemaphore_, portMAX_DELAY) != pdPASS) {
        log_e("Failed to take display update");
        return;
    }

    SHOW_WATERMARK();
    ESP_ERROR_CHECK(
        esp_lcd_panel_draw_bitmap(panelHandle_, 0, 0, nativeWidth(), nativeHeight(), getBuffer()));
    SHOW_WATERMARK();

    clearFrameBuffer_ = clearFrameBuffer;

    log_i("DisplayEsp32P4SeeYa049::updateDisplay()");
}

#endif