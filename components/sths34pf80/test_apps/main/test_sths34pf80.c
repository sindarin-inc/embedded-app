/*
 * SPDX-FileCopyrightText: 2025 Sindarin Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <inttypes.h>
#include <string.h>

#include "driver/gpio.h"
#include "driver/i2c_master.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "sths34pf80.h"
#include "unity.h"
#include "unity_test_runner.h"
#include "unity_test_utils_memory.h"

#define TEST_DELAY_TIME_MS (60000)

static const char *TAG = "sths34pf80_test";
static i2c_master_bus_handle_t bus_handle = NULL;
static i2c_master_dev_handle_t dev_handle = NULL;

static void test_i2c_init(void) {
    // Initialize I2C bus
    i2c_master_bus_config_t bus_config = {
        .i2c_port = CONFIG_TEST_I2C_PORT_NUM,
        .sda_io_num = CONFIG_TEST_I2C_SDA_GPIO,
        .scl_io_num = CONFIG_TEST_I2C_SCL_GPIO,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };
    TEST_ESP_OK(i2c_new_master_bus(&bus_config, &bus_handle));

    // Add device to the bus
    TEST_ESP_OK(sths34pf80_i2c_init(bus_handle, CONFIG_TEST_I2C_DEVICE_ADDR, &dev_handle));
}

static void i2c_scan_bus(void) {
    ESP_LOGI(TAG, "Scanning I2C bus for devices...");
    uint8_t found_count = 0;

    for (uint8_t addr = 0x08; addr < 0x78; addr++) {
        i2c_device_config_t dev_cfg = {
            .dev_addr_length = I2C_ADDR_BIT_LEN_7,
            .device_address = addr,
            .scl_speed_hz = 100000, // Use slower speed for scanning
        };

        i2c_master_dev_handle_t temp_dev = NULL;
        esp_err_t ret = i2c_master_bus_add_device(bus_handle, &dev_cfg, &temp_dev);
        if (ret != ESP_OK) {
            continue;
        }

        // Try to read a register (WHO_AM_I at 0x0F)
        uint8_t reg = STHS34PF80_WHO_AM_I;
        uint8_t data;
        // Disable I2C logging to avoid spamming the console for expected failures
        esp_log_level_t log_level = esp_log_level_get("i2c.master");
        esp_log_level_set("i2c.master", ESP_LOG_NONE);
        ret = i2c_master_transmit_receive(temp_dev, &reg, 1, &data, 1, 10);
        esp_log_level_set("i2c.master", log_level);

        if (ret == ESP_OK) {
            ESP_LOGI(TAG, "Found device at address 0x%02X", addr);
            found_count++;
        }

        i2c_master_bus_rm_device(temp_dev);
    }

    ESP_LOGI(TAG, "I2C scan complete. Found %d device(s)", found_count);
}

static void test_i2c_deinit(void) {
    TEST_ESP_OK(sths34pf80_i2c_deinit(dev_handle));
    TEST_ESP_OK(i2c_del_master_bus(bus_handle));
    bus_handle = NULL;
    dev_handle = NULL;
}

TEST_CASE("test i2c bus scan", "[sths34pf80][scan]") {
    ESP_LOGI(TAG, "Initializing I2C bus for scanning");

    // Initialize just the bus without the device
    i2c_master_bus_config_t bus_config = {
        .i2c_port = CONFIG_TEST_I2C_PORT_NUM,
        .sda_io_num = CONFIG_TEST_I2C_SDA_GPIO,
        .scl_io_num = CONFIG_TEST_I2C_SCL_GPIO,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };
    TEST_ESP_OK(i2c_new_master_bus(&bus_config, &bus_handle));

    // Add delay to let sensor power up
    vTaskDelay(pdMS_TO_TICKS(100));

    i2c_scan_bus();

    TEST_ESP_OK(i2c_del_master_bus(bus_handle));
    bus_handle = NULL;
}

TEST_CASE("test sths34pf80 initialization", "[sths34pf80][init]") {
    ESP_LOGI(TAG, "Initializing STHS34PF80 sensor");
    test_i2c_init();

    uint8_t whoami;
    TEST_ESP_OK(sths34pf80_read_reg(dev_handle, STHS34PF80_WHO_AM_I, &whoami));
    ESP_LOGI(TAG, "WHO_AM_I: 0x%02X (expected 0x%02X)", whoami, STHS34PF80_WHOAMI_VALUE);
    TEST_ASSERT_EQUAL_HEX8(STHS34PF80_WHOAMI_VALUE, whoami);

    test_i2c_deinit();
}

TEST_CASE("test sths34pf80 temperature data reading", "[sths34pf80][temp_read]") {
    ESP_LOGI(TAG, "Reading temperature data from STHS34PF80");
    test_i2c_init();

    TEST_ESP_OK(sths34pf80_write_reg(dev_handle, STHS34PF80_CTRL1,
                                     STHS34PF80_CTRL1_BDU_MASK | STHS34PF80_ODR_1_HZ));

    uint8_t status;
    int retry_count = 0;
    do {
        TEST_ESP_OK(sths34pf80_read_reg(dev_handle, STHS34PF80_STATUS, &status));
        if (!(status & STHS34PF80_STATUS_DRDY_MASK)) {
            vTaskDelay(pdMS_TO_TICKS(100));
            retry_count++;
        }
    } while (!(status & STHS34PF80_STATUS_DRDY_MASK) && retry_count < 20);

    if (status & STHS34PF80_STATUS_DRDY_MASK) {
        uint8_t temp_data[2];
        TEST_ESP_OK(sths34pf80_read_bytes(dev_handle, STHS34PF80_TOBJECT_L, temp_data, 2));
        int16_t object_temp = (int16_t)(temp_data[1] << 8 | temp_data[0]);
        ESP_LOGI(TAG, "Object temperature raw value: %d", object_temp);

        TEST_ESP_OK(sths34pf80_read_bytes(dev_handle, STHS34PF80_TAMBIENT_L, temp_data, 2));
        int16_t ambient_temp = (int16_t)(temp_data[1] << 8 | temp_data[0]);
        ESP_LOGI(TAG, "Ambient temperature value: %0.1fºC", ambient_temp / 100.0);
    } else {
        ESP_LOGW(TAG, "Data not ready after waiting");
    }

    TEST_ESP_OK(sths34pf80_write_reg(dev_handle, STHS34PF80_CTRL1, STHS34PF80_ODR_POWER_DOWN));

    test_i2c_deinit();
}

TEST_CASE("test sths34pf80 presence detection", "[sths34pf80][presence]") {
    ESP_LOGI(TAG, "Testing presence detection");
    test_i2c_init();

    uint16_t presence_threshold = 1000;
    TEST_ESP_OK(sths34pf80_write_embedded_register(dev_handle, STHS34PF80_PRESENCE_THS_L,
                                                   presence_threshold & 0xFF));
    TEST_ESP_OK(sths34pf80_write_embedded_register(dev_handle, STHS34PF80_PRESENCE_THS_H,
                                                   presence_threshold >> 8));
    TEST_ESP_OK(sths34pf80_reset_algorithms(dev_handle));

    uint16_t motion_threshold = 1000;
    TEST_ESP_OK(sths34pf80_write_embedded_register(dev_handle, STHS34PF80_MOTION_THS_L,
                                                   motion_threshold & 0xFF));
    TEST_ESP_OK(sths34pf80_write_embedded_register(dev_handle, STHS34PF80_MOTION_THS_H,
                                                   motion_threshold >> 8));

    TEST_ESP_OK(sths34pf80_write_reg(dev_handle, STHS34PF80_CTRL1,
                                     STHS34PF80_CTRL1_BDU_MASK | STHS34PF80_ODR_2_HZ));

    ESP_LOGI(TAG, "Monitoring presence for %d seconds...", TEST_DELAY_TIME_MS / 1000);

    for (int i = 0; i < (TEST_DELAY_TIME_MS / 500); i++) {
        uint8_t status;
        TEST_ESP_OK(sths34pf80_read_reg(dev_handle, STHS34PF80_STATUS, &status));

        if (status & STHS34PF80_STATUS_DRDY_MASK) {
            uint8_t presence_data[2];
            TEST_ESP_OK(
                sths34pf80_read_bytes(dev_handle, STHS34PF80_TPRESENCE_L, presence_data, 2));
            int16_t presence_value = (int16_t)(presence_data[1] << 8 | presence_data[0]);

            uint8_t motion_data[2];
            TEST_ESP_OK(sths34pf80_read_bytes(dev_handle, STHS34PF80_TMOTION_L, motion_data, 2));
            int16_t motion_value = (int16_t)(motion_data[1] << 8 | motion_data[0]);

            uint8_t func_status;
            TEST_ESP_OK(sths34pf80_read_reg(dev_handle, STHS34PF80_FUNC_STATUS, &func_status));

            ESP_LOGI(TAG, "Presence: %d (flag=%d), Motion: %d (flag=%d)", presence_value,
                     (func_status & 0x04) ? 1 : 0, motion_value, (func_status & 0x02) ? 1 : 0);
        }

        vTaskDelay(pdMS_TO_TICKS(500));
    }

    TEST_ESP_OK(sths34pf80_write_reg(dev_handle, STHS34PF80_CTRL1, STHS34PF80_ODR_POWER_DOWN));

    test_i2c_deinit();
}

static bool interrupt_triggered = false;
static void IRAM_ATTR interrupt_handler(void *arg) { interrupt_triggered = true; }

TEST_CASE("test sths34pf80 presence detection interrupt", "[sths34pf80][presence_int]") {
    ESP_LOGI(TAG, "Testing presence detection interrupt");
    test_i2c_init();

    // Reboot
    TEST_ESP_OK(sths34pf80_reboot_and_wait(dev_handle));

    uint16_t presence_threshold = 1000;
    TEST_ESP_OK(sths34pf80_write_embedded_register(dev_handle, STHS34PF80_PRESENCE_THS_L,
                                                   presence_threshold & 0xFF));
    TEST_ESP_OK(sths34pf80_write_embedded_register(dev_handle, STHS34PF80_PRESENCE_THS_H,
                                                   presence_threshold >> 8));

    uint16_t motion_threshold = 1000;
    TEST_ESP_OK(sths34pf80_write_embedded_register(dev_handle, STHS34PF80_MOTION_THS_L,
                                                   motion_threshold & 0xFF));
    TEST_ESP_OK(sths34pf80_write_embedded_register(dev_handle, STHS34PF80_MOTION_THS_H,
                                                   motion_threshold >> 8));

    TEST_ESP_OK(sths34pf80_write_reg(dev_handle, STHS34PF80_CTRL1,
                                     STHS34PF80_CTRL1_BDU_MASK | STHS34PF80_ODR_2_HZ));

    // INT_H_L[7] Active low/high (1 = active low)
    // PP_OD [6] Push-pull/open-drain (1 = open drain)
    // INT_MSK2[5] Presence interrupt mask (1 = enabled)
    // INT_MSK1[4] Motion interrupt mask (1 = enabled)
    // INT_MSK0[3] Ambient temperature interrupt mask (1 = enabled)
    // INT_LATCHED[2] Latch interrupt (1 = latched, 0 = pulsed)
    // IEN[1:0] Configures the signal routed to the INT pin (0 = high Z, 1 = DRDY, 2 = INT_OR)
    uint8_t ctrl3 = 0xE2;
    TEST_ESP_OK(sths34pf80_write_reg(dev_handle, STHS34PF80_CTRL3, ctrl3));

    gpio_config_t io_conf = {
        .pin_bit_mask = 1ULL << CONFIG_TEST_INTERRUPT_GPIO,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_NEGEDGE,
    };
    gpio_config(&io_conf);

    gpio_install_isr_service(0);
    gpio_isr_handler_add(CONFIG_TEST_INTERRUPT_GPIO, interrupt_handler, NULL);

    ESP_LOGI(TAG, "Wating for interrupt for %d seconds...", TEST_DELAY_TIME_MS / 1000);

    int timeout_ms = TEST_DELAY_TIME_MS;
    while (!interrupt_triggered && timeout_ms > 0) {
        vTaskDelay(pdMS_TO_TICKS(100));
        timeout_ms -= 100;
    }

    if (interrupt_triggered) {
        ESP_LOGI(TAG, "Interrupt triggered");
    }

    gpio_isr_handler_remove(CONFIG_TEST_INTERRUPT_GPIO);
    gpio_uninstall_isr_service();
    gpio_reset_pin(CONFIG_TEST_INTERRUPT_GPIO);

    TEST_ESP_OK(sths34pf80_write_reg(dev_handle, STHS34PF80_CTRL1, STHS34PF80_ODR_POWER_DOWN));
    test_i2c_deinit();

    if (!interrupt_triggered) {
        ESP_LOGE(TAG, "Interrupt not triggered");
        TEST_FAIL();
    }
}

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
    printf(" ____ _____ _   _ ____ _____ _  _   ____  _____ ___   ___  \n");
    printf("/ ___|_   _| | | / ___|___ /| || | |  _ \\|  ___( _ ) / _ \\ \n");
    printf("\\___ \\ | | | |_| \\___ \\ |_ \\| || |_| |_) | |_  / _ \\| | | |\n");
    printf(" ___) || | |  _  |___) |__) |__   _|  __/|  _|| (_) | |_| |\n");
    printf("|____/ |_| |_| |_|____/____/   |_| |_|   |_|   \\___/ \\___/ \n");
    printf("\n");
    printf("STHS34PF80 Human Presence Sensor Test Application\n");
    printf("I2C Port: %d, SCL: GPIO%d, SDA: GPIO%d, Interrupt: GPIO%d, Address: 0x%02X\n",
           CONFIG_TEST_I2C_PORT_NUM, CONFIG_TEST_I2C_SCL_GPIO, CONFIG_TEST_I2C_SDA_GPIO,
           CONFIG_TEST_INTERRUPT_GPIO, CONFIG_TEST_I2C_DEVICE_ADDR);
    unity_run_menu();
}