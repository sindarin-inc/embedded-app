#if CONFIG_USE_ICM42670

#include "Icm42670.hpp"

#include <esp_check.h>
#include <esp_log.h>
#include <freertos/FreeRTOS.h>

#include "sindarin-debug.h"

constexpr const char *TAG = "ICM42670";

auto Icm42670::begin(i2c_master_bus_handle_t i2cBusHandle) -> esp_err_t {
    ESP_RETURN_ON_ERROR(icm42670_create(i2cBusHandle, ICM42670_I2C_ADDRESS, &icm42670_), TAG,
                        "Failed to create ICM42670");
    icm42670_cfg_t imu_cfg = {
        .acce_fs = ACCE_FS_2G,
        .acce_odr = ACCE_ODR_400HZ,
        .gyro_fs = GYRO_FS_2000DPS,
        .gyro_odr = GYRO_ODR_400HZ,
    };
    ESP_RETURN_ON_ERROR(icm42670_config(icm42670_, &imu_cfg), TAG, "Failed to configure ICM42670");
    ESP_RETURN_ON_ERROR(icm42670_acce_set_pwr(icm42670_, ACCE_PWR_LOWNOISE), TAG,
                        "Failed to set accelerometer power mode");

    return ESP_OK;
}

auto Icm42670::end() -> esp_err_t {
    ESP_RETURN_ON_ERROR(icm42670_acce_set_pwr(icm42670_, ACCE_PWR_OFF), TAG,
                        "Failed to set accelerometer power mode");
    icm42670_delete(icm42670_);
    return ESP_OK;
}

auto Icm42670::readDeviceId(uint8_t *deviceIdOut) -> esp_err_t {
    return icm42670_get_deviceid(icm42670_, deviceIdOut);
}

auto Icm42670::clearInterruptStatus() -> esp_err_t {
    uint8_t int_status = 0;
    ESP_RETURN_ON_ERROR(icm42670_read_register(icm42670_, ICM42670_INT_STATUS, &int_status), TAG,
                        "Failed to read INT_STATUS");
    uint8_t int_status_2 = 0;
    ESP_RETURN_ON_ERROR(icm42670_read_register(icm42670_, ICM42670_INT_STATUS2, &int_status_2), TAG,
                        "Failed to read INT_STATUS2");
    uint8_t int_status_3 = 0;
    ESP_RETURN_ON_ERROR(icm42670_read_register(icm42670_, ICM42670_INT_STATUS3, &int_status_3), TAG,
                        "Failed to read INT_STATUS3");
    log_d("INT_STATUS: 0x%02x INT_STATUS2: 0x%02x INT_STATUS3: 0x%02x", int_status, int_status_2,
          int_status_3);
    return ESP_OK;
}

auto Icm42670::configureInterrupts() -> esp_err_t {
    // Reset
    // err = icm42670_write_register(icm42670, ICM42670_SIGNAL_PATH_RESET, 0x10);
    // if (err != ESP_OK) {
    //     ESP_LOGE(TAG, "Failed to icm42670_write_register: %s", esp_err_to_name(err));
    // }
    // vTaskDelay(pdMS_TO_TICKS(10));
    // err = icm42670_write_register(icm42670, ICM42670_SIGNAL_PATH_RESET, 0x00);
    // if (err != ESP_OK) {
    //     ESP_LOGE(TAG, "Failed to icm42670_write_register: %s", esp_err_to_name(err));
    // }
    // vTaskDelay(pdMS_TO_TICKS(10));

    // Configure ICM42670 Wake on Motion interrupt

    // // ICM42670_INT_CONFIG (default: 0x00)
    // INT1_MODE[2] (0=pulse, 1=latch)
    // INT1_DRIVE_CIRCUIT[1] (0=open-drain, 1=push-pull)
    // INT1_POLARITY[0] (0=active low, 1=active high)
    uint8_t int_config = 0x04;
    ESP_RETURN_ON_ERROR(icm42670_write_register(icm42670_, ICM42670_INT_CONFIG, int_config), TAG,
                        "Failed to configure INT_CONFIG");

    // WOM_INT_DUR[3:4] = 0 WoM interrupt asserted at first overthreshold event
    // WOM_INT_MODE[2] = 1 WoM interrupt on the AND
    // WOM_MODE[1] = 1 compare to previous samples
    // WOM_EN[0] = 1 enable WOM
    uint8_t wom_config = 0x07;
    ESP_RETURN_ON_ERROR(icm42670_write_register(icm42670_, ICM42670_WOM_CONFIG, wom_config), TAG,
                        "Failed to configure WOM");

    // I3C_PROTOCOL_ERROR_INT1_EN[6] = 0 I3CSM protocol error interrupt
    // SMD_INT1_EN[3] = 0
    // WOM_Z_INT1_EN[2] = 0
    // WOM_Y_INT1_EN[1] = 1
    // WOM_X_INT1_EN[0] = 0
    uint8_t int_source1 = 0x02;
    ESP_RETURN_ON_ERROR(icm42670_write_register(icm42670_, ICM42670_INT_SOURCE1, int_source1), TAG,
                        "Failed to configure INT_SOURCE1");

    // ICM42670_MREG1_INT_CONFIG1 (default: 0x10)
    // INT_TPULSE_DURATION[6] = 0 Interrupt pulse duration is 100 µs
    // INT_ASYNC_RESET[4] = 1 The interrupt pulse remains high for the intended duration
    // independent of when the interrupt status register is read
    ESP_RETURN_ON_ERROR(
        icm42670_write_mreg_register(icm42670_, 1, ICM42670_MREG1_INT_CONFIG1, 0x00), TAG,
        "Failed to configure INT_CONFIG1");

    // Set WOM threshold for Y axis (195mg threshold)
    // Register value = threshold / 3.9mg (for ±2g range)
    vTaskDelay(1);
    uint8_t wom_threshold = 50; // ~195mg
    ESP_RETURN_ON_ERROR(
        icm42670_write_mreg_register(icm42670_, 1, ICM42670_MREG1_ACCEL_WOM_Y_THR, wom_threshold),
        TAG, "Failed to set WOM Y threshold");

    // Read back the mreg
    wom_threshold = 0;
    ESP_RETURN_ON_ERROR(
        icm42670_read_mreg_register(icm42670_, 1, ICM42670_MREG1_ACCEL_WOM_Y_THR, &wom_threshold),
        TAG, "Failed to read WOM Y threshold");

    ESP_LOGI(TAG, "Read WOM threshold as %" PRIu8, wom_threshold);

    // Clear interrupt status
    ESP_RETURN_ON_ERROR(clearInterruptStatus(), TAG, "Failed to clear interrupt status");

    return ESP_OK;
}

auto Icm42670::readAccelerometer(icm42670_value_t *value) -> esp_err_t {
    return icm42670_get_acce_value(icm42670_, value);
}

#endif
