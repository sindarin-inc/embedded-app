/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "soc/soc_caps.h"

#if SOC_MIPI_DSI_SUPPORTED
#include <inttypes.h>

#include "driver/gpio.h"
#include "driver/i2c.h"
#include "driver/spi_master.h"
#include "esp_dma_utils.h"
#include "esp_heap_caps.h"
#include "esp_lcd_mipi_dsi.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_seeya049.h"
#include "esp_ldo_regulator.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "unity.h"
#include "unity_test_runner.h"
#include "unity_test_utils_memory.h"

#define TEST_LCD_H_RES (1920)
#define TEST_LCD_V_RES (1080)
#define TEST_LCD_BIT_PER_PIXEL (24)
#define TEST_PIN_NUM_LCD_RST (23)
#define TEST_MIPI_DSI_LANE_NUM (2)

#if TEST_LCD_BIT_PER_PIXEL == 24
#define TEST_MIPI_DPI_PX_FORMAT (LCD_COLOR_PIXEL_FORMAT_RGB888)
#elif TEST_LCD_BIT_PER_PIXEL == 18
#define TEST_MIPI_DPI_PX_FORMAT (LCD_COLOR_PIXEL_FORMAT_RGB666)
#elif TEST_LCD_BIT_PER_PIXEL == 16
#define TEST_MIPI_DPI_PX_FORMAT (LCD_COLOR_PIXEL_FORMAT_RGB565)
#endif

#define TEST_DELAY_TIME_MS (3000)

// Required for the MIPI peripheral
#define TEST_MIPI_DSI_PHY_PWR_LDO_CHAN (3)
#define TEST_MIPI_DSI_PHY_PWR_LDO_VOLTAGE_MV (2500)

static char *TAG = "seeya049_test";
static esp_ldo_channel_handle_t ldo_mipi_phy = NULL;
static esp_lcd_panel_handle_t panel_handle = NULL;
static esp_lcd_dsi_bus_handle_t mipi_dsi_bus = NULL;
static esp_lcd_panel_io_handle_t mipi_dbi_io = NULL;
static SemaphoreHandle_t refresh_finish = NULL;

IRAM_ATTR static bool test_notify_refresh_ready(esp_lcd_panel_handle_t panel,
                                                esp_lcd_dpi_panel_event_data_t *edata,
                                                void *user_ctx) {
    SemaphoreHandle_t refresh_finish = (SemaphoreHandle_t)user_ctx;
    BaseType_t need_yield = pdFALSE;

    xSemaphoreGiveFromISR(refresh_finish, &need_yield);

    return (need_yield == pdTRUE);
}

static void test_init_mipi_dsi_bus(void) {
    // Enable debug logging for lcd.dsi.dpi
    esp_log_level_set("lcd.dsi.dpi", ESP_LOG_DEBUG);

    // Turn on the power for MIPI DSI PHY, so it can go from "No Power" state to "Shutdown" state
#ifdef TEST_MIPI_DSI_PHY_PWR_LDO_CHAN
    ESP_LOGI(TAG, "MIPI DSI PHY Powered on");
    esp_ldo_channel_config_t ldo_mipi_phy_config = {
        .chan_id = TEST_MIPI_DSI_PHY_PWR_LDO_CHAN,
        .voltage_mv = TEST_MIPI_DSI_PHY_PWR_LDO_VOLTAGE_MV,
    };
    TEST_ESP_OK(esp_ldo_acquire_channel(&ldo_mipi_phy_config, &ldo_mipi_phy));
#endif

    ESP_LOGI(TAG, "Initialize MIPI DSI bus");
    esp_lcd_dsi_bus_config_t bus_config = {
        .bus_id = 0,
        .num_data_lanes = TEST_MIPI_DSI_LANE_NUM,
        .phy_clk_src = MIPI_DSI_PHY_CLK_SRC_DEFAULT,
        // This doesn't seem to affect the size of what is drawn
        // lane_bit_rate_mbps / 8 / 18 must be > 1 and < 256
        .lane_bit_rate_mbps = 1000,
    };

    TEST_ESP_OK(esp_lcd_new_dsi_bus(&bus_config, &mipi_dsi_bus));

    ESP_LOGI(TAG, "Install panel IO");
    esp_lcd_dbi_io_config_t dbi_config = {
        .virtual_channel = 0,
        .lcd_cmd_bits = 8,
        .lcd_param_bits = 16,
    };
    TEST_ESP_OK(esp_lcd_new_panel_io_dbi(mipi_dsi_bus, &dbi_config, &mipi_dbi_io));
}

static void test_new_seeya_panel(void) {
    test_init_mipi_dsi_bus();

    ESP_LOGI(TAG, "Install LCD driver of seeya049");
    esp_lcd_dpi_panel_config_t dpi_config = {
        .virtual_channel = 0,
        .dpi_clk_src = MIPI_DSI_DPI_CLK_SRC_DEFAULT,
        // This seems to affect the size and quality of what is drawn
        // This can't keep up above 120MHz
        // Below 35MHz the drawn screen area is reduced
        .dpi_clock_freq_mhz = 80,
        .pixel_format = TEST_MIPI_DPI_PX_FORMAT,
        .in_color_format = 0,
        .out_color_format = 0,
        .num_fbs = 1,
        .video_timing =
            {
                .h_size = TEST_LCD_H_RES,
                .v_size = TEST_LCD_V_RES,
                // From GPT
                // .hsync_pulse_width = 10,
                // .hsync_back_porch = 20,
                // .hsync_front_porch = 20,
                // .vsync_pulse_width = 4,
                // .vsync_back_porch = 16,
                // .vsync_front_porch = 16,

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
                .dsi_bus = mipi_dsi_bus,
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

    TEST_ESP_OK(esp_lcd_new_panel_seeya049(mipi_dbi_io, &panel_config, &panel_handle));
}

static void test_init_enable_panel(void) {
    test_new_seeya_panel();

    TEST_ESP_OK(esp_lcd_panel_reset(panel_handle));
    TEST_ESP_OK(esp_lcd_panel_init(panel_handle));
    TEST_ESP_OK(esp_lcd_panel_disp_on_off(panel_handle, true));

    refresh_finish = xSemaphoreCreateBinary();
    TEST_ASSERT_NOT_NULL(refresh_finish);
    esp_lcd_dpi_panel_event_callbacks_t cbs = {
        .on_color_trans_done = test_notify_refresh_ready,
    };
    TEST_ESP_OK(esp_lcd_dpi_panel_register_event_callbacks(panel_handle, &cbs, refresh_finish));
}

static void test_deinit_mipi_dsi_bus(void) {
    TEST_ESP_OK(esp_lcd_panel_io_del(mipi_dbi_io));
    TEST_ESP_OK(esp_lcd_del_dsi_bus(mipi_dsi_bus));
    mipi_dbi_io = NULL;
    mipi_dsi_bus = NULL;
    if (ldo_mipi_phy) {
        TEST_ESP_OK(esp_ldo_release_channel(ldo_mipi_phy));
        ldo_mipi_phy = NULL;
    }
}

static void test_deinit_seeya_panel(void) {
    TEST_ESP_OK(esp_lcd_panel_del(panel_handle));
    panel_handle = NULL;
    test_deinit_mipi_dsi_bus();
}

static void test_deinit_disable_panel(void) {
    test_deinit_seeya_panel();

    vSemaphoreDelete(refresh_finish);
    refresh_finish = NULL;
}

TEST_CASE("test seeya049 to draw pre-set patterns with MIPI interface",
          "[seeya049][draw_pattern]") {
    ESP_LOGI(TAG, "Initialize LCD device");
    test_init_enable_panel();

    ESP_LOGI(TAG, "Show color bar pattern drawn by hardware");
    ESP_LOGI(TAG, "Vertical");
    TEST_ESP_OK(esp_lcd_dpi_panel_set_pattern(panel_handle, MIPI_DSI_PATTERN_BAR_VERTICAL));
    vTaskDelay(pdMS_TO_TICKS(TEST_DELAY_TIME_MS));

    ESP_LOGI(TAG, "Horizontal");
    TEST_ESP_OK(esp_lcd_dpi_panel_set_pattern(panel_handle, MIPI_DSI_PATTERN_BAR_HORIZONTAL));
    vTaskDelay(pdMS_TO_TICKS(TEST_DELAY_TIME_MS));

    ESP_LOGI(TAG, "Vertical BER");
    TEST_ESP_OK(esp_lcd_dpi_panel_set_pattern(panel_handle, MIPI_DSI_PATTERN_BER_VERTICAL));
    vTaskDelay(pdMS_TO_TICKS(TEST_DELAY_TIME_MS));

    TEST_ESP_OK(esp_lcd_dpi_panel_set_pattern(panel_handle, MIPI_DSI_PATTERN_NONE));

    ESP_LOGI(TAG, "Deinitialize LCD device");
    test_deinit_disable_panel();
}

static void test_draw_color_bar(esp_lcd_panel_handle_t panel_handle, uint16_t h_res,
                                uint16_t v_res) {
    uint8_t byte_per_pixel = (TEST_LCD_BIT_PER_PIXEL + 7) / 8;
    uint8_t *color = (uint8_t *)malloc(h_res * v_res * byte_per_pixel);
    TEST_ASSERT_NOT_NULL(color);

    uint8_t red[] = {0x00, 0x00, 0xff};
    uint8_t green[] = {0x00, 0xff, 0x00};
    uint8_t blue[] = {0xff, 0x00, 0x00};
    uint8_t *colors[3] = {red, green, blue};

    uint8_t bar_count = 10;

    // Draw horizontal color bars
    for (int bar = 0; bar < bar_count; bar++) {
        int color_idx = bar % 3; // Cycle through red, green, blue
        int start = (bar * h_res * v_res) / bar_count;
        int end = ((bar + 1) * h_res * v_res) / bar_count;

        for (int i = start; i < end; i++) {
            color[i * byte_per_pixel + 0] = colors[color_idx][0];
            color[i * byte_per_pixel + 1] = colors[color_idx][1];
            color[i * byte_per_pixel + 2] = colors[color_idx][2];
        }
    }

    TEST_ESP_OK(esp_lcd_panel_draw_bitmap(panel_handle, 0, 0, h_res, v_res, color));
    xSemaphoreTake(refresh_finish, portMAX_DELAY);

    free(color);
}

static void test_draw_box(esp_lcd_panel_handle_t panel_handle, uint16_t x, uint16_t y,
                          uint16_t width, uint16_t height, uint8_t *color) {
    uint8_t byte_per_pixel = (TEST_LCD_BIT_PER_PIXEL + 7) / 8;
    uint8_t *color_buffer = (uint8_t *)malloc(width * height * byte_per_pixel);
    TEST_ASSERT_NOT_NULL(color_buffer);

    for (int i = 0; i < width * height; i++) {
        color_buffer[i * byte_per_pixel + 0] = color[0];
        color_buffer[i * byte_per_pixel + 1] = color[1];
        color_buffer[i * byte_per_pixel + 2] = color[2];
    }

    TEST_ESP_OK(esp_lcd_panel_draw_bitmap(panel_handle, x, y, x + width, y + height, color_buffer));
    xSemaphoreTake(refresh_finish, portMAX_DELAY);

    free(color_buffer);
}

TEST_CASE("test seeya049 to draw color bars rendered from software", "[seeya049][draw_color_bar]") {
    ESP_LOGI(TAG, "Initialize LCD device");
    test_init_enable_panel();

    ESP_LOGI(TAG, "Show color bar drawn by software");
    test_draw_color_bar(panel_handle, TEST_LCD_H_RES, TEST_LCD_V_RES);
    vTaskDelay(pdMS_TO_TICKS(TEST_DELAY_TIME_MS));

    ESP_LOGI(TAG, "Deinitialize LCD device");
    test_deinit_disable_panel();
}

static void test_draw_full_screen_colors(esp_lcd_panel_handle_t panel_handle, uint16_t h_res,
                                         uint16_t v_res, uint8_t *fillColor) {
    uint8_t byte_per_pixel = (TEST_LCD_BIT_PER_PIXEL + 7) / 8;
    uint8_t *color = (uint8_t *)malloc(h_res * v_res * byte_per_pixel);
    TEST_ASSERT_NOT_NULL(color);

    for (int i = 0; i < h_res * v_res; i++) {
        color[i * byte_per_pixel + 0] = fillColor[0];
        color[i * byte_per_pixel + 1] = fillColor[1];
        color[i * byte_per_pixel + 2] = fillColor[2];
    }

    TEST_ESP_OK(esp_lcd_panel_draw_bitmap(panel_handle, 0, 0, h_res, v_res, color));
    xSemaphoreTake(refresh_finish, portMAX_DELAY);

    free(color);
}

TEST_CASE("test seeya049 to mirror with MIPI interface", "[seeya049][mirror]") {
    ESP_LOGI(TAG, "Initialize LCD device");
    test_init_enable_panel();

    uint8_t red[] = {0x00, 0x00, 0xff};
    uint8_t green[] = {0x00, 0xff, 0x00};
    uint8_t blue[] = {0xff, 0x00, 0x00};
    uint8_t white[] = {0xFF, 0xFF, 0xFF};

    ESP_LOGI(TAG, "Mirror the screen");
    for (size_t i = 0; i < 4; i++) {
        bool mirror_x = i & 2;
        bool mirror_y = i & 1;
        ESP_LOGI(TAG, "Mirror X: %d, Mirror Y: %d", mirror_x, mirror_y);
        TEST_ESP_OK(esp_lcd_panel_mirror(panel_handle, mirror_x, mirror_y));
        // Draw boxes in the corners
        test_draw_box(panel_handle, 0, 0, 300, 300, red);
        test_draw_box(panel_handle, TEST_LCD_H_RES - 300, 0, 300, 300, green);
        test_draw_box(panel_handle, 0, TEST_LCD_V_RES - 300, 300, 300, blue);
        test_draw_box(panel_handle, TEST_LCD_H_RES - 300, TEST_LCD_V_RES - 300, 300, 300, white);
        vTaskDelay(pdMS_TO_TICKS(TEST_DELAY_TIME_MS));
    }

    ESP_LOGI(TAG, "Deinitialize LCD device");
    test_deinit_disable_panel();
}

TEST_CASE("test seeya049 to draw full screen colors with MIPI interface",
          "[seeya049][draw_full_screen_colors]") {
    ESP_LOGI(TAG, "Initialize LCD device");
    test_init_enable_panel();

    uint8_t red[] = {0x00, 0x00, 0xff};
    uint8_t green[] = {0x00, 0xff, 0x00};
    uint8_t blue[] = {0xff, 0x00, 0x00};

    ESP_LOGI(TAG, "Show red screen drawn by software");
    test_draw_full_screen_colors(panel_handle, TEST_LCD_H_RES, TEST_LCD_V_RES, red);
    vTaskDelay(pdMS_TO_TICKS(TEST_DELAY_TIME_MS));

    ESP_LOGI(TAG, "Show green screen drawn by software");
    test_draw_full_screen_colors(panel_handle, TEST_LCD_H_RES, TEST_LCD_V_RES, green);
    vTaskDelay(pdMS_TO_TICKS(TEST_DELAY_TIME_MS));

    ESP_LOGI(TAG, "Show blue screen drawn by software");
    test_draw_full_screen_colors(panel_handle, TEST_LCD_H_RES, TEST_LCD_V_RES, blue);
    vTaskDelay(pdMS_TO_TICKS(TEST_DELAY_TIME_MS));

    ESP_LOGI(TAG, "Deinitialize LCD device");
    test_deinit_disable_panel();
}

TEST_CASE("test seeya049 debug command to turn on all pixels", "[seeya049][all_pixels_on]") {
    ESP_LOGI(TAG, "Initialize LCD device");
    test_init_enable_panel();

    panel_seeya049_all_pixels_on_off(panel_handle, true);
    vTaskDelay(pdMS_TO_TICKS(TEST_DELAY_TIME_MS));

    panel_seeya049_all_pixels_on_off(panel_handle, false);
    vTaskDelay(pdMS_TO_TICKS(TEST_DELAY_TIME_MS));

    ESP_LOGI(TAG, "Deinitialize LCD device");
    test_deinit_disable_panel();
}

TEST_CASE("test seeya049 self test", "[seeya049][self_test]") {
    ESP_LOGI(TAG, "Initialize LCD device");
    test_new_seeya_panel();

    panel_seeya049_run_selftest(panel_handle);
    // Self-test cycles back to red at 39 seconds
    vTaskDelay(pdMS_TO_TICKS(40000));

    esp_lcd_panel_reset(panel_handle);
    test_deinit_seeya_panel();
}

TEST_CASE("test seeya049 draw grid of boxes", "[seeya049][draw_grid_of_boxes]") {
    ESP_LOGI(TAG, "Initialize LCD device");
    test_init_enable_panel();

    // Box dimensions
    const uint16_t box_width = TEST_LCD_H_RES;
    const uint16_t box_height = TEST_LCD_V_RES;

    uint8_t byte_per_pixel = (TEST_LCD_BIT_PER_PIXEL + 7) / 8;
    uint8_t *color = (uint8_t *)malloc(box_width * box_height * byte_per_pixel);
    ESP_LOGI(TAG, "Allocated color buffer of size %d", box_width * box_height * byte_per_pixel);
    TEST_ASSERT_NOT_NULL(color);

    // Fill with gray color
    for (int i = 0; i < box_width * box_height; i++) {
        color[i * byte_per_pixel + 0] = 0x08; // Blue
        color[i * byte_per_pixel + 1] = 0x08; // Green
        color[i * byte_per_pixel + 2] = 0x08; // Red
    }

    for (int x = 0; x < box_width; x += 100) {
        for (int y = 0; y < box_height; y++) {
            color[(y * box_width + x) * byte_per_pixel + 0] = 0x0F; // Blue
            color[(y * box_width + x) * byte_per_pixel + 1] = 0xB0; // Green
            color[(y * box_width + x) * byte_per_pixel + 2] = 0x0F; // Red
        }
    }

    for (int y = 0; y < box_height; y += 100) {
        for (int x = 0; x < box_width; x++) {
            color[(y * box_width + x) * byte_per_pixel + 0] = 0x0F; // Blue
            color[(y * box_width + x) * byte_per_pixel + 1] = 0x0F; // Green
            color[(y * box_width + x) * byte_per_pixel + 2] = 0xB0; // Red
        }
    }

    // Vertical line on the left edge
    for (int x = 0; x < box_width; x++) {
        color[(0 * box_width + x) * byte_per_pixel + 0] = 0xFF; // Blue
        color[(0 * box_width + x) * byte_per_pixel + 1] = 0x00; // Green
        color[(0 * box_width + x) * byte_per_pixel + 2] = 0x00; // Red
    }

    // Horizontal line on the top edge
    for (int y = 0; y < box_height; y++) {
        color[(y * box_width + 0) * byte_per_pixel + 0] = 0xFF; // Blue
        color[(y * box_width + 0) * byte_per_pixel + 1] = 0x00; // Green
        color[(y * box_width + 0) * byte_per_pixel + 2] = 0x00; // Red
    }

    // Horizontal line on the bottom edge
    for (int x = 0; x < box_width; x++) {
        color[((box_height - 1) * box_width + x) * byte_per_pixel + 0] = 0xFF; // Blue
        color[((box_height - 1) * box_width + x) * byte_per_pixel + 1] = 0x00; // Green
        color[((box_height - 1) * box_width + x) * byte_per_pixel + 2] = 0x00; // Red
    }

    // Vertical line on the right edge
    for (int y = 0; y < box_height; y++) {
        color[(y * box_width + (box_width - 1)) * byte_per_pixel + 0] = 0xFF; // Blue
        color[(y * box_width + (box_width - 1)) * byte_per_pixel + 1] = 0x00; // Green
        color[(y * box_width + (box_width - 1)) * byte_per_pixel + 2] = 0x00; // Red
    }

    TEST_ESP_OK(esp_lcd_panel_draw_bitmap(panel_handle, 0, 0, box_width, box_height, color));
    xSemaphoreTake(refresh_finish, portMAX_DELAY);

    vTaskDelay(pdMS_TO_TICKS(TEST_DELAY_TIME_MS));

    free(color);

    ESP_LOGI(TAG, "Deinitialize LCD device");
    test_deinit_disable_panel();
}

TEST_CASE("test seeya049 draw gradients", "[seeya049][draw_gradients]") {
    ESP_LOGI(TAG, "Initialize LCD device");
    test_init_enable_panel();

    // Box dimensions
    const uint16_t box_width = TEST_LCD_H_RES;
    const uint16_t box_height = TEST_LCD_V_RES;

    uint8_t byte_per_pixel = (TEST_LCD_BIT_PER_PIXEL + 7) / 8;
    uint8_t *color = (uint8_t *)malloc(box_width * box_height * byte_per_pixel);
    ESP_LOGI(TAG, "Allocated color buffer of size %d", box_width * box_height * byte_per_pixel);
    TEST_ASSERT_NOT_NULL(color);

    // Draw green gradient
    ESP_LOGI(TAG, "Drawing green gradient");
    for (int y = 0; y < box_height; y++) {
        uint8_t intensity = (uint8_t)(y * 255 / box_height);
        for (int x = 0; x < box_width; x++) {
            color[(y * box_width + x) * byte_per_pixel + 0] = 0x0F;      // Blue
            color[(y * box_width + x) * byte_per_pixel + 1] = intensity; // Green
            color[(y * box_width + x) * byte_per_pixel + 2] = 0x0F;      // Red
        }
    }
    TEST_ESP_OK(esp_lcd_panel_draw_bitmap(panel_handle, 0, 0, box_width, box_height, color));
    xSemaphoreTake(refresh_finish, portMAX_DELAY);
    vTaskDelay(pdMS_TO_TICKS(TEST_DELAY_TIME_MS));

    // Draw red gradient
    ESP_LOGI(TAG, "Drawing red gradient");
    for (int y = 0; y < box_height; y++) {
        uint8_t intensity = (uint8_t)(y * 255 / box_height);
        for (int x = 0; x < box_width; x++) {
            color[(y * box_width + x) * byte_per_pixel + 0] = 0x0F;      // Blue
            color[(y * box_width + x) * byte_per_pixel + 1] = 0x0F;      // Green
            color[(y * box_width + x) * byte_per_pixel + 2] = intensity; // Red
        }
    }
    TEST_ESP_OK(esp_lcd_panel_draw_bitmap(panel_handle, 0, 0, box_width, box_height, color));
    xSemaphoreTake(refresh_finish, portMAX_DELAY);
    vTaskDelay(pdMS_TO_TICKS(TEST_DELAY_TIME_MS));

    // Draw blue gradient
    ESP_LOGI(TAG, "Drawing blue gradient");
    for (int y = 0; y < box_height; y++) {
        uint8_t intensity = (uint8_t)(y * 255 / box_height);
        for (int x = 0; x < box_width; x++) {
            color[(y * box_width + x) * byte_per_pixel + 0] = intensity; // Blue
            color[(y * box_width + x) * byte_per_pixel + 1] = 0x0F;      // Green
            color[(y * box_width + x) * byte_per_pixel + 2] = 0x0F;      // Red
        }
    }
    TEST_ESP_OK(esp_lcd_panel_draw_bitmap(panel_handle, 0, 0, box_width, box_height, color));
    xSemaphoreTake(refresh_finish, portMAX_DELAY);
    vTaskDelay(pdMS_TO_TICKS(TEST_DELAY_TIME_MS));

    free(color);

    ESP_LOGI(TAG, "Deinitialize LCD device");
    test_deinit_disable_panel();
}

TEST_CASE("test seeya049 direct RAM write", "[seeya049][write_ram]") {
    ESP_LOGI(TAG, "Initialize LCD device");
    test_new_seeya_panel();
    TEST_ESP_OK(esp_lcd_panel_reset(panel_handle));
    TEST_ESP_OK(panel_seeya049_send_init_cmds(panel_handle));
    // TEST_ESP_OK(esp_lcd_panel_disp_on_off(panel_handle, true));

    // Create a small test pattern (10x10 pixels, RGB888 format)
    // NOTE: We get stuck if we try to send more than 1000 bytes. I suspect it has something to do
    // with the MIPI driver since I'm not seeing anything come out from the logic analyzer.
    const uint16_t pattern_width = 30;
    uint16_t pattern_height = 11;
    const uint8_t bytes_per_pixel = 3; // RGB888
    uint8_t *pattern =
        heap_caps_malloc(pattern_width * pattern_height * bytes_per_pixel, MALLOC_CAP_DMA);
    TEST_ASSERT_NOT_NULL(pattern);

    // Fill pattern with alternating red and blue pixels
    for (int y = 0; y < pattern_height; y++) {
        for (int x = 0; x < pattern_width; x++) {
            int pixel_idx = (y * pattern_width + x) * bytes_per_pixel;
            if ((x + y) % 2 == 0) {
                // Red pixel
                pattern[pixel_idx + 0] = 0x00; // B
                pattern[pixel_idx + 1] = 0x00; // G
                pattern[pixel_idx + 2] = 0xFF; // R
            } else {
                // Blue pixel
                pattern[pixel_idx + 0] = 0xFF; // B
                pattern[pixel_idx + 1] = 0x00; // G
                pattern[pixel_idx + 2] = 0x00; // R
            }
        }
    }

    // Write pattern to display RAM
    ESP_LOGI(TAG, "Writing test pattern directly to display RAM");
    TEST_ESP_OK(panel_seeya049_write_ram(panel_handle, pattern,
                                         pattern_width * pattern_height * bytes_per_pixel));

    vTaskDelay(pdMS_TO_TICKS(TEST_DELAY_TIME_MS));

    free(pattern);

    ESP_LOGI(TAG, "Deinitialize LCD device");
    test_deinit_seeya_panel();
}

esp_err_t hardware_reset_seeya049(void) {
    // Config the reset pin and pull it high
    gpio_config_t io_conf = {
        .mode = GPIO_MODE_OUTPUT_OD,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
        .pin_bit_mask = 1ULL << TEST_PIN_NUM_LCD_RST,
    };
    TEST_ESP_OK(gpio_config(&io_conf));
    TEST_ESP_OK(gpio_set_level(TEST_PIN_NUM_LCD_RST, 1));
    vTaskDelay(pdMS_TO_TICKS(1));
    TEST_ESP_OK(gpio_set_level(TEST_PIN_NUM_LCD_RST, 0));
    vTaskDelay(pdMS_TO_TICKS(1));
    TEST_ESP_OK(gpio_set_level(TEST_PIN_NUM_LCD_RST, 1));
    vTaskDelay(pdMS_TO_TICKS(120));

    return ESP_OK;
}

TEST_CASE("test seeya049 I2C self-test", "[seeya049][i2c_self_test]") {
    hardware_reset_seeya049();

    // Run the self-test
    ESP_LOGI(TAG, "Running I2C self-test");
    TEST_ESP_OK(panel_seeya049_run_selftest_i2c());

    vTaskDelay(pdMS_TO_TICKS(TEST_DELAY_TIME_MS));
}

TEST_CASE("test seeya049 I2C framebuffer test", "[seeya049][i2c_framebuffer_test]") {
    hardware_reset_seeya049();

    // Run the framebuffer test
    ESP_LOGI(TAG, "Running I2C framebuffer test");

    const uint16_t pattern_width = 1920;
    const uint16_t pattern_height = 1080;
    const uint8_t bytes_per_pixel = 3; // RGB888
    uint8_t *pattern = malloc(pattern_width * pattern_height * bytes_per_pixel);

    // Fill pattern with alternating red and blue pixels
    for (int y = 0; y < pattern_height; y++) {
        for (int x = 0; x < pattern_width; x++) {
            int pixel_idx = (y * pattern_width + x) * bytes_per_pixel;
            if ((x + y) % 2 == 0) {
                // Red pixel
                pattern[pixel_idx + 0] = 0x00; // B
                pattern[pixel_idx + 1] = 0x00; // G
                pattern[pixel_idx + 2] = 0xFF; // R
            } else {
                // Blue pixel
                pattern[pixel_idx + 0] = 0xFF; // B
                pattern[pixel_idx + 1] = 0x00; // G
                pattern[pixel_idx + 2] = 0x00; // R
            }
        }
    }

    TEST_ESP_OK(panel_seeya049_run_framebuffer_test_i2c(pattern, pattern_width * pattern_height *
                                                                     bytes_per_pixel));

    free(pattern);
}

#endif

// Some resources are lazy allocated in the LCD driver, the threadhold is left for that case
#define TEST_MEMORY_LEAK_THRESHOLD (300)

static size_t before_free_8bit;
static size_t before_free_32bit;

void setUp(void) {
    before_free_8bit = heap_caps_get_free_size(MALLOC_CAP_8BIT);
    before_free_32bit = heap_caps_get_free_size(MALLOC_CAP_32BIT);
}

void tearDown(void) {
    size_t after_free_8bit = heap_caps_get_free_size(MALLOC_CAP_8BIT);
    size_t after_free_32bit = heap_caps_get_free_size(MALLOC_CAP_32BIT);
    unity_utils_check_leak(before_free_8bit, after_free_8bit, "8BIT", TEST_MEMORY_LEAK_THRESHOLD);
    unity_utils_check_leak(before_free_32bit, after_free_32bit, "32BIT",
                           TEST_MEMORY_LEAK_THRESHOLD);
}

void app_main(void) {
    /**
     *    _____        __     __      ___  _  _   ___  _ _
     *   / ____|       \ \   / /     / _ \| || | / _ \( | )
     *  | (___   ___  __\ \_/ /_ _  | | | | || || (_) |V V
     *   \___ \ / _ \/ _ \   / _` | | | | |__   _\__, |
     *   ____) |  __/  __/| | (_| | | |_| |  | |   / /
     *  |_____/ \___|\___||_|\__,_|  \___(_) |_|  /_/
     */
    printf("   _____        __     __      ___  _  _   ___  _ _\r\n");
    printf("  / ____|       \\ \\   / /     / _ \\| || | / _ \\( | )\r\n");
    printf(" | (___   ___  __\\ \\_/ /_ _  | | | | || || (_) |V V\r\n");
    printf("  \\___ \\ / _ \\/ _ \\   / _` | | | | |__   _\\__, |\r\n");
    printf("  ____) |  __/  __/| | (_| | | |_| |  | |   / /\r\n");
    printf(" |_____/ \\___|\\___||_|\\__,_|  \\___(_) |_|  /_/\r\n");
    unity_run_menu();
}
