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
#include "esp_lcd_seeya103.h"
#include "esp_ldo_regulator.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "unity.h"
#include "unity_test_runner.h"
#include "unity_test_utils_memory.h"

#define TEST_LCD_H_RES (1280)
#define TEST_LCD_V_RES (1280)
#define TEST_LCD_BIT_PER_PIXEL (24)

#define TEST_PIN_NUM_LCD_RST (23)
#define TEST_PIN_NUM_I2C_SDA (22)
#define TEST_PIN_NUM_I2C_SCL (21)

#define TEST_DELAY_TIME_MS (3000)

// Required for the MIPI peripheral
#define TEST_MIPI_DSI_PHY_PWR_LDO_CHAN (3)
#define TEST_MIPI_DSI_PHY_PWR_LDO_VOLTAGE_MV (2500)

static char *TAG = "seeya103_test";
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
    esp_lcd_dsi_bus_config_t bus_config = panel_seeya103_dsi_bus_config();

    TEST_ESP_OK(esp_lcd_new_dsi_bus(&bus_config, &mipi_dsi_bus));

    ESP_LOGI(TAG, "Install panel IO");
    esp_lcd_dbi_io_config_t dbi_config = panel_seeya103_io_dbi_config();
    TEST_ESP_OK(esp_lcd_new_panel_io_dbi(mipi_dsi_bus, &dbi_config, &mipi_dbi_io));
}

static void test_new_seeya_panel(seeya103_vendor_config_t *vendor_config) {
    test_init_mipi_dsi_bus();

    esp_lcd_dpi_panel_config_t dpi_config = panel_seeya103_1280_1280_dpi_config();
    seeya103_vendor_config_t default_vendor_config =
        panel_seeya103_vendor_config(mipi_dsi_bus, &dpi_config, TEST_PIN_NUM_LCD_RST);
    if (vendor_config == NULL) {
        vendor_config = &default_vendor_config;
    }

    ESP_LOGI(TAG, "Install LCD driver of seeya103");
    vendor_config->mipi_config.dsi_bus = mipi_dsi_bus;
    const esp_lcd_panel_dev_config_t panel_config = panel_seeya103_panel_config(vendor_config);

    TEST_ESP_OK(esp_lcd_new_panel_seeya103(mipi_dbi_io, &panel_config, &panel_handle));

    refresh_finish = xSemaphoreCreateBinary();
    TEST_ASSERT_NOT_NULL(refresh_finish);
    esp_lcd_dpi_panel_event_callbacks_t cbs = {
        .on_color_trans_done = test_notify_refresh_ready,
    };
    TEST_ESP_OK(esp_lcd_dpi_panel_register_event_callbacks(panel_handle, &cbs, refresh_finish));
}

static void test_init_enable_panel(seeya103_vendor_config_t *vendor_config) {
    test_new_seeya_panel(vendor_config);

    TEST_ESP_OK(esp_lcd_panel_reset(panel_handle));
    TEST_ESP_OK(esp_lcd_panel_init(panel_handle));
    TEST_ESP_OK(esp_lcd_panel_disp_on_off(panel_handle, true));
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

TEST_CASE("test seeya103 to draw pre-set patterns with MIPI interface",
          "[seeya103][draw_pattern]") {
    ESP_LOGI(TAG, "Initialize LCD device");
    test_init_enable_panel(NULL);

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

TEST_CASE("test seeya103 to draw color bars rendered from software", "[seeya103][draw_color_bar]") {
    ESP_LOGI(TAG, "Initialize LCD device");
    test_init_enable_panel(NULL);

    ESP_LOGI(TAG, "Show color bar drawn by software");
    test_draw_color_bar(panel_handle, TEST_LCD_H_RES, TEST_LCD_V_RES);
    vTaskDelay(pdMS_TO_TICKS(TEST_DELAY_TIME_MS));

    ESP_LOGI(TAG, "Deinitialize LCD device");
    test_deinit_disable_panel();
}

TEST_CASE("test seeya103 to mirror with MIPI interface", "[seeya103][mirror]") {
    ESP_LOGI(TAG, "Initialize LCD device");
    test_init_enable_panel(NULL);

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

TEST_CASE("test seeya103 to draw full screen colors with MIPI interface",
          "[seeya103][draw_full_screen_colors]") {
    ESP_LOGI(TAG, "Initialize LCD device");
    test_init_enable_panel(NULL);

    ESP_LOGI(TAG, "Show red screen drawn by software");
    uint8_t red[] = {0x00, 0x00, 0xff};
    test_draw_box(panel_handle, 0, 0, TEST_LCD_H_RES, TEST_LCD_V_RES, red);
    vTaskDelay(pdMS_TO_TICKS(TEST_DELAY_TIME_MS));

    ESP_LOGI(TAG, "Show green screen drawn by software");
    uint8_t green[] = {0x00, 0xff, 0x00};
    test_draw_box(panel_handle, 0, 0, TEST_LCD_H_RES, TEST_LCD_V_RES, green);
    vTaskDelay(pdMS_TO_TICKS(TEST_DELAY_TIME_MS));

    ESP_LOGI(TAG, "Show blue screen drawn by software");
    uint8_t blue[] = {0xff, 0x00, 0x00};
    test_draw_box(panel_handle, 0, 0, TEST_LCD_H_RES, TEST_LCD_V_RES, blue);
    vTaskDelay(pdMS_TO_TICKS(TEST_DELAY_TIME_MS));

    ESP_LOGI(TAG, "Deinitialize LCD device");
    test_deinit_disable_panel();
}

TEST_CASE("test seeya103 debug command to turn on all pixels", "[seeya103][all_pixels_on]") {
    ESP_LOGI(TAG, "Initialize LCD device");
    test_init_enable_panel(NULL);

    panel_seeya103_all_pixels_on_off(panel_handle, true);
    vTaskDelay(pdMS_TO_TICKS(TEST_DELAY_TIME_MS));

    panel_seeya103_all_pixels_on_off(panel_handle, false);

    ESP_LOGI(TAG, "Deinitialize LCD device");
    test_deinit_disable_panel();
}

TEST_CASE("test seeya103 self test", "[seeya103][self_test]") {
    ESP_LOGI(TAG, "Initialize LCD device");
    test_new_seeya_panel(NULL);

    panel_seeya103_run_selftest(panel_handle);
    vTaskDelay(pdMS_TO_TICKS(TEST_DELAY_TIME_MS));

    esp_lcd_panel_reset(panel_handle);
    test_deinit_seeya_panel();
}

TEST_CASE("test seeya103 draw grid of boxes", "[seeya103][draw_grid_of_boxes]") {
    ESP_LOGI(TAG, "Initialize LCD device");
    test_init_enable_panel(NULL);

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

TEST_CASE("test seeya103 draw gradients", "[seeya103][draw_gradients]") {
    ESP_LOGI(TAG, "Initialize LCD device");
    test_init_enable_panel(NULL);

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

TEST_CASE("test seeya103 moving display area", "[seeya103][moving_display_area]") {
    ESP_LOGI(TAG, "Initialize LCD device");
    esp_lcd_dpi_panel_config_t dpi_config = panel_seeya103_1280_1280_dpi_config();
    seeya103_vendor_config_t vendor_config =
        panel_seeya103_vendor_config(mipi_dsi_bus, &dpi_config, TEST_PIN_NUM_LCD_RST);
    vendor_config.scaling = SEEYA103_SCALING_NONE;
    test_init_enable_panel(&vendor_config);

    const uint16_t pattern_width = 128;
    const uint16_t pattern_height = 128;

    TEST_ESP_OK(esp_lcd_dpi_panel_set_pattern(panel_handle, MIPI_DSI_PATTERN_BAR_VERTICAL));
    vTaskDelay(pdMS_TO_TICKS(TEST_DELAY_TIME_MS));

    TEST_ESP_OK(panel_seeya103_i2c_init(TEST_PIN_NUM_I2C_SDA, TEST_PIN_NUM_I2C_SCL));

    // Move the display area to the right
    TEST_ESP_OK(panel_seeya103_move_display_area(panel_handle, pattern_width, pattern_height));
    // TEST_ESP_OK(panel_seeya103_move_display_area_i2c(pattern_width, pattern_height));
    vTaskDelay(pdMS_TO_TICKS(TEST_DELAY_TIME_MS));

    // Move the display area to the left
    TEST_ESP_OK(
        panel_seeya103_move_display_area(panel_handle, 2 * pattern_width, 2 * pattern_height));
    // TEST_ESP_OK(panel_seeya103_move_display_area_i2c(2 * pattern_width, 2 * pattern_height));
    vTaskDelay(pdMS_TO_TICKS(TEST_DELAY_TIME_MS));

    TEST_ESP_OK(panel_seeya103_i2c_deinit());

    test_deinit_disable_panel();
}

esp_err_t hardware_reset_seeya103(void) {
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

TEST_CASE("test seeya103 I2C self-test", "[seeya103][i2c_self_test]") {
    TEST_ESP_OK(panel_seeya103_i2c_init(TEST_PIN_NUM_I2C_SDA, TEST_PIN_NUM_I2C_SCL));
    // Run the self-test
    ESP_LOGI(TAG, "Running I2C self-test");
    TEST_ESP_OK(panel_seeya103_send_cmds_i2c(
        TEST_LCD_H_RES, TEST_LCD_V_RES, 0, 0, SEEYA103_SCALING_2X, seeya103_init_cmds_selftest,
        sizeof(seeya103_init_cmds_selftest) / sizeof(seeya103_lcd_init_cmd_t)));

    vTaskDelay(pdMS_TO_TICKS(TEST_DELAY_TIME_MS));
    hardware_reset_seeya103();

    TEST_ESP_OK(panel_seeya103_i2c_deinit());
}

TEST_CASE("test seeya103 I2C config with MIPI display", "[seeya103][i2c_config]") {
    hardware_reset_seeya103();
    panel_seeya103_i2c_init(TEST_PIN_NUM_I2C_SDA, TEST_PIN_NUM_I2C_SCL);

    uint16_t x_offset = (2560 - TEST_LCD_H_RES) / 2;
    uint16_t y_offset = (2560 - TEST_LCD_V_RES) / 2;
    panel_seeya103_send_cmds_i2c(TEST_LCD_H_RES, TEST_LCD_V_RES, x_offset, y_offset,
                                 SEEYA103_SCALING_NONE, seeya103_init_cmds,
                                 sizeof(seeya103_init_cmds) / sizeof(seeya103_lcd_init_cmd_t));
    panel_seeya103_disp_on_off_i2c(true);

    // Init the MIPI driver but with no INIT commands via the MIPI driver
    esp_lcd_dpi_panel_config_t dpi_config = panel_seeya103_1280_1280_dpi_config();
    seeya103_vendor_config_t vendor_config =
        panel_seeya103_vendor_config(mipi_dsi_bus, &dpi_config, -1);
    vendor_config.scaling = SEEYA103_SCALING_NONE;
    vendor_config.init_cmds = NULL;
    vendor_config.init_cmds_size = 0;
    test_new_seeya_panel(&vendor_config);
    TEST_ESP_OK(esp_lcd_panel_init(panel_handle));

    // Original position
    TEST_ESP_OK(esp_lcd_dpi_panel_set_pattern(panel_handle, MIPI_DSI_PATTERN_BAR_VERTICAL));
    vTaskDelay(pdMS_TO_TICKS(TEST_DELAY_TIME_MS));

    hardware_reset_seeya103();
    test_deinit_disable_panel();
    panel_seeya103_i2c_deinit();
}

TEST_CASE("test centered with no scaling", "[seeya103][centered_no_scaling]") {
    esp_lcd_dpi_panel_config_t dpi_config = panel_seeya103_1280_1280_dpi_config();
    seeya103_vendor_config_t vendor_config =
        panel_seeya103_vendor_config(mipi_dsi_bus, &dpi_config, TEST_PIN_NUM_LCD_RST);
    // Centered not scaled
    vendor_config.x_offset = (2560 - dpi_config.video_timing.h_size) / 2;
    vendor_config.y_offset = (2560 - dpi_config.video_timing.v_size) / 2;
    vendor_config.scaling = SEEYA103_SCALING_NONE;
    test_init_enable_panel(&vendor_config);

    // Original position
    TEST_ESP_OK(esp_lcd_dpi_panel_set_pattern(panel_handle, MIPI_DSI_PATTERN_BAR_VERTICAL));
    vTaskDelay(pdMS_TO_TICKS(TEST_DELAY_TIME_MS));

    TEST_ESP_OK(esp_lcd_dpi_panel_set_pattern(panel_handle, MIPI_DSI_PATTERN_BAR_HORIZONTAL));
    vTaskDelay(pdMS_TO_TICKS(TEST_DELAY_TIME_MS));

    TEST_ESP_OK(esp_lcd_dpi_panel_set_pattern(panel_handle, MIPI_DSI_PATTERN_BER_VERTICAL));
    vTaskDelay(pdMS_TO_TICKS(TEST_DELAY_TIME_MS));

    test_deinit_disable_panel();
}

static void run_scaling_test(seeya103_scaling_t scaling) {
    ESP_LOGI(TAG, "Testing with %s scaling",
             scaling == SEEYA103_SCALING_NONE    ? "1x"
             : scaling == SEEYA103_SCALING_1_33X ? "1.33x"
                                                 : "2x");

    esp_lcd_dpi_panel_config_t dpi_config = panel_seeya103_1280_1280_dpi_config();
    dpi_config.video_timing.h_size = 640;
    dpi_config.video_timing.v_size = 640;
    int display_width = dpi_config.video_timing.h_size;
    int display_height = dpi_config.video_timing.v_size;
    if (scaling == SEEYA103_SCALING_1_33X) {
        display_width = display_width * 4 / 3;
        display_height = display_height * 4 / 3;
    } else if (scaling == SEEYA103_SCALING_2X) {
        display_width = display_width * 2;
        display_height = display_height * 2;
    }
    seeya103_vendor_config_t vendor_config =
        panel_seeya103_vendor_config(mipi_dsi_bus, &dpi_config, TEST_PIN_NUM_LCD_RST);
    vendor_config.x_offset = (2560 - display_width) / 2;
    vendor_config.y_offset = (2560 - display_height) / 2;
    vendor_config.scaling = scaling;
    test_init_enable_panel(&vendor_config);

    TEST_ESP_OK(esp_lcd_dpi_panel_set_pattern(panel_handle, MIPI_DSI_PATTERN_BAR_HORIZONTAL));
    vTaskDelay(pdMS_TO_TICKS(TEST_DELAY_TIME_MS / 2));
    // Show vertical pattern
    TEST_ESP_OK(esp_lcd_dpi_panel_set_pattern(panel_handle, MIPI_DSI_PATTERN_BAR_VERTICAL));
    vTaskDelay(pdMS_TO_TICKS(TEST_DELAY_TIME_MS / 2));

    test_deinit_disable_panel();
}

TEST_CASE("test reduced resolution with scaling", "[seeya103][reduced_resolution_with_scaling]") {
    // Test all scaling factors
    run_scaling_test(SEEYA103_SCALING_NONE);
    run_scaling_test(SEEYA103_SCALING_1_33X);
    run_scaling_test(SEEYA103_SCALING_2X);
}

#endif

// Some resources are lazy allocated in the LCD driver, the threadhold is left for that case
#define TEST_MEMORY_LEAK_THRESHOLD (300)

static size_t before_free_8bit;
static size_t before_free_32bit;

void setUp(void) {
    before_free_8bit = heap_caps_get_free_size(MALLOC_CAP_8BIT);
    before_free_32bit = heap_caps_get_free_size(MALLOC_CAP_32BIT);
    esp_log_level_set("lcd.dsi.dpi", ESP_LOG_DEBUG);
    esp_log_level_set("seeya103", ESP_LOG_DEBUG);
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
