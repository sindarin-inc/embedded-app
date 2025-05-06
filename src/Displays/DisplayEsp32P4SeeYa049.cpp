#if CONFIG_DISPLAY_ESP32_P4_SEEYA_049

#include "DisplayEsp32P4SeeYa049.hpp"

#include <driver/gpio.h>
#include <esp_check.h>
#include <esp_lcd_seeya049.h>

#define TEST_MIPI_DSI_PHY_PWR_LDO_CHAN (3)
#define TEST_MIPI_DSI_PHY_PWR_LDO_VOLTAGE_MV (2500)
#define TEST_MIPI_DSI_LANE_NUM (2)
#define TEST_LCD_H_RES (1920)
#define TEST_LCD_V_RES (1080)
#define TEST_LCD_BIT_PER_PIXEL (24)
#define TEST_PIN_NUM_LCD_RST (23)
#define TEST_MIPI_DPI_PX_FORMAT (LCD_COLOR_PIXEL_FORMAT_RGB888)

IRAM_ATTR bool DisplayEsp32P4SeeYa049::onColorTransDone(esp_lcd_panel_handle_t panel,
                                                        esp_lcd_dpi_panel_event_data_t *edata,
                                                        void *user_ctx) {
    auto display = static_cast<DisplayEsp32P4SeeYa049 *>(user_ctx);
    xSemaphoreGiveFromISR(display->updateSemaphore_, nullptr);
    return true;
}

auto DisplayEsp32P4SeeYa049::setup() -> bool {
    DisplaySystem::setup();
    invertDisplay(true);

    updateSemaphore_ = xSemaphoreCreateBinary();

    // Enable debug logging for lcd.dsi.dpi
    esp_log_level_set("lcd.dsi.dpi", ESP_LOG_DEBUG);

    // Turn on the power for MIPI DSI PHY, so it can go from "No Power" state to "Shutdown" state
    log_d("MIPI DSI PHY Powered on");
    esp_ldo_channel_config_t ldo_mipi_phy_config = {
        .chan_id = TEST_MIPI_DSI_PHY_PWR_LDO_CHAN,
        .voltage_mv = TEST_MIPI_DSI_PHY_PWR_LDO_VOLTAGE_MV,
    };
    ESP_ERROR_CHECK(esp_ldo_acquire_channel(&ldo_mipi_phy_config, &ldoMipiPhy_));

    log_d("Initialize MIPI DSI bus");
    esp_lcd_dsi_bus_config_t bus_config = {
        .bus_id = 0,
        .num_data_lanes = TEST_MIPI_DSI_LANE_NUM,
        .phy_clk_src = MIPI_DSI_PHY_CLK_SRC_DEFAULT,
        // This doesn't seem to affect the size of what is drawn
        // lane_bit_rate_mbps / 8 / 18 must be > 1 and < 256
        .lane_bit_rate_mbps = 1000,
    };

    ESP_ERROR_CHECK(esp_lcd_new_dsi_bus(&bus_config, &mipiDsiBus_));

    log_d("Install panel IO");
    esp_lcd_dbi_io_config_t dbi_config = {
        .virtual_channel = 0,
        .lcd_cmd_bits = 8,
        .lcd_param_bits = 16,
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_dbi(mipiDsiBus_, &dbi_config, &mipiDbiIo_));

    log_d("Install LCD driver of seeya049");
    esp_lcd_dpi_panel_config_t dpi_config = {
        .virtual_channel = 0,
        .dpi_clk_src = MIPI_DSI_DPI_CLK_SRC_DEFAULT,
        .dpi_clock_freq_mhz = 80,
        .pixel_format = TEST_MIPI_DPI_PX_FORMAT,
        .in_color_format = static_cast<lcd_color_format_t>(0),
        .out_color_format = static_cast<lcd_color_format_t>(0),
        .num_fbs = 1,
        .video_timing =
            {
                .h_size = TEST_LCD_H_RES,
                .v_size = TEST_LCD_V_RES,
                // From the NED init code spreadsheet
                .hsync_pulse_width = 44,
                .hsync_back_porch = 148,
                .hsync_front_porch = 88,
                .vsync_pulse_width = 5,
                .vsync_back_porch = 36,
                .vsync_front_porch = 5,
            },
        .flags =
            {
                .use_dma2d = true,
                .disable_lp = false,
            },
    };

    seeya049_vendor_config_t vendor_config = {
        .mipi_config =
            {
                .dsi_bus = mipiDsiBus_,
                .dpi_config = &dpi_config,
                .lane_num = TEST_MIPI_DSI_LANE_NUM,
            },
    };

    const esp_lcd_panel_dev_config_t panel_config = {
        .reset_gpio_num = TEST_PIN_NUM_LCD_RST,
        // These don't have an effect on the test patterns
        .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_BGR,
        .data_endian = LCD_RGB_DATA_ENDIAN_BIG,
        .bits_per_pixel = TEST_LCD_BIT_PER_PIXEL,
        .flags =
            {
                .reset_active_high = false,
            },
        .vendor_config = &vendor_config,
    };

    ESP_ERROR_CHECK(esp_lcd_new_panel_seeya049(mipiDbiIo_, &panel_config, &panelHandle_));

    ESP_ERROR_CHECK(esp_lcd_panel_reset(panelHandle_));
    ESP_ERROR_CHECK(esp_lcd_panel_init(panelHandle_));
    ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(panelHandle_, true));
    log_i("DisplayEsp32P4SeeYa049::setup() done with size %d x %d", nativeWidth(), nativeHeight());

    esp_lcd_dpi_panel_event_callbacks_t cbs = {
        .on_color_trans_done = onColorTransDone,
    };
    ESP_ERROR_CHECK(esp_lcd_dpi_panel_register_event_callbacks(panelHandle_, &cbs, this));

    return true;
}

void DisplayEsp32P4SeeYa049::updateDisplay(DisplaySystem::UpdateType updateType,
                                           bool clearFrameBuffer, int vcomNudge,
                                           DisplaySystem::UpdateType altUpdateType,
                                           ScreenSide screen, SolRect altRect) {

    ESP_ERROR_CHECK(
        esp_lcd_panel_draw_bitmap(panelHandle_, 0, 0, nativeWidth(), nativeHeight(), getBuffer()));

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

#endif