#include "Ltr329Als.hpp"

#include <cstdio>
#include <driver/i2c.h>
#include <esp_log.h>

#include "sindarin-debug.h"

#define I2C_MASTER_TIMEOUT_MS 1000
#define LTR329ALS01_SENSOR_ADDR 0x29

typedef struct {
    union {
        uint8_t raw;
        // Least-significant bit first in a union
        struct {
            // 1 = Active mode, 0 = Stand-by mode
            uint8_t alsMode : 1;
            // 1 = Initial start-up procedure is started, bit has default value of 0 after start-up
            // 0 = Initial start-up procedure is NOT started (default)
            uint8_t swReset : 1;
            // 000 Gain 1x (1 lux to 64k lux) (default)
            // 001 Gain 2x (0.5 lux to 32k lux)
            // 010 Gain 4x (0.25 lux to 16k lux)
            // 011 Gain 8x (0.125 lux to 8k lux)
            // 100 Reserved
            // 101 Reserved
            // 110 Gain 48x (0.02 lux to 1.3k lux)
            // 111 Gain 96x (0.01 lux to 600 lux)
            uint8_t alsGain : 3;
            // MUST write to 000 (default)
            uint8_t reserved : 3;
        };
    };
} ltr329als01_als_contr_value;

auto Ltr329Als::registerReadByte(ltr329als01_reg_t regAddr, uint8_t *data) -> esp_err_t {
    return i2c_master_write_read_device(i2cNum_, LTR329ALS01_SENSOR_ADDR, (uint8_t *)&regAddr, 1,
                                        data, 1, I2C_MASTER_TIMEOUT_MS / portTICK_RATE_MS);
}

auto Ltr329Als::registerWriteByte(ltr329als01_reg_t regAddr, uint8_t data) -> esp_err_t {
    int ret;
    uint8_t writeBuf[2] = {static_cast<uint8_t>(regAddr), data};
    ret = i2c_master_write_to_device(i2cNum_, LTR329ALS01_SENSOR_ADDR, writeBuf, sizeof(writeBuf),
                                     I2C_MASTER_TIMEOUT_MS / portTICK_RATE_MS);

    return ret;
}

auto Ltr329Als::readPartId() -> uint8_t {
    uint8_t partId = 0;
    auto err = registerReadByte(LTR329ALS01_REG_PART_ID, &partId);
    if (err != ESP_OK) {
        ESP_LOGE("LTR329ALS", "Failed to read part ID (%s)", esp_err_to_name(err));
    }
    return partId;
}

auto Ltr329Als::readManufacturerId() -> uint8_t {
    uint8_t manufacturerId = 0;
    auto err = registerReadByte(LTR329ALS01_REG_MANUFAC_ID, &manufacturerId);
    if (err != ESP_OK) {
        ESP_LOGE("LTR329ALS", "Failed to read part ID (%s)", esp_err_to_name(err));
    }
    return manufacturerId;
}

auto Ltr329Als::enable() -> bool {
    log_d("Starting Ltr329Als");
    // Turn on
    ltr329als01_als_contr_value control;
    control.raw = 0;
    control.alsMode = 1;
    // control.sw_reset = 1;
    esp_err_t err;
    err = registerWriteByte(LTR329ALS01_REG_ALS_CONTR, control.raw);
    if (err != ESP_OK) {
        ESP_LOGE("LTR329ALS", "Failed to write control register (%s)", esp_err_to_name(err));
        return false;
    }
    return true;
}

auto Ltr329Als::disable() -> bool {
    // TODO: Turn off
    return true;
}

auto Ltr329Als::readStatus() -> ltr329als01_status_value {
    ltr329als01_status_value status;
    ESP_ERROR_CHECK(registerReadByte(LTR329ALS01_REG_ALS_STATUS, &(status.raw)));
    return status;
}

auto Ltr329Als::readChannel0() -> ltr329als01_als_data_value {
    ltr329als01_als_data_value dataCh0;
    ESP_ERROR_CHECK(registerReadByte(LTR329ALS01_REG_ALS_DATA_CH0_0, dataCh0.raw));
    ESP_ERROR_CHECK(registerReadByte(LTR329ALS01_REG_ALS_DATA_CH0_1, dataCh0.raw + 1));
    return dataCh0;
}

auto Ltr329Als::readChannel1() -> ltr329als01_als_data_value {
    ltr329als01_als_data_value dataCh1;
    ESP_ERROR_CHECK(registerReadByte(LTR329ALS01_REG_ALS_DATA_CH1_0, dataCh1.raw));
    ESP_ERROR_CHECK(registerReadByte(LTR329ALS01_REG_ALS_DATA_CH1_1, dataCh1.raw + 1));
    return dataCh1;
}
