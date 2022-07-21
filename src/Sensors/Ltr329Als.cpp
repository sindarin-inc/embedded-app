#include "Ltr329Als.hpp"

#include <driver/i2c.h>
#include <esp_log.h>
#include <stdio.h>

#define I2C_MASTER_TIMEOUT_MS 1000
#define LTR329ALS01_SENSOR_ADDR 0x29

typedef struct {
    union {
        uint8_t raw;
        // Least-significant bit first in a union
        struct {
            // 1 = Active mode, 0 = Stand-by mode
            uint8_t als_mode : 1;
            // 1 = Initial start-up procedure is started, bit has default value of 0 after start-up
            // 0 = Initial start-up procedure is NOT started (default)
            uint8_t sw_reset : 1;
            // 000 Gain 1x (1 lux to 64k lux) (default)
            // 001 Gain 2x (0.5 lux to 32k lux)
            // 010 Gain 4x (0.25 lux to 16k lux)
            // 011 Gain 8x (0.125 lux to 8k lux)
            // 100 Reserved
            // 101 Reserved
            // 110 Gain 48x (0.02 lux to 1.3k lux)
            // 111 Gain 96x (0.01 lux to 600 lux)
            uint8_t als_gain : 3;
            // MUST write to 000 (default)
            uint8_t reserved : 3;
        };
    };
} ltr329als01_als_contr_value;

esp_err_t Ltr329Als::registerReadByte(ltr329als01_reg_t reg_addr, uint8_t *data) {
    return i2c_master_write_read_device(i2cNum_, LTR329ALS01_SENSOR_ADDR, (uint8_t *)&reg_addr, 1,
                                        data, 1, I2C_MASTER_TIMEOUT_MS / portTICK_RATE_MS);
}

esp_err_t Ltr329Als::registerWriteByte(ltr329als01_reg_t reg_addr, uint8_t data) {
    int ret;
    uint8_t write_buf[2] = {reg_addr, data};
    ret = i2c_master_write_to_device(i2cNum_, LTR329ALS01_SENSOR_ADDR, write_buf, sizeof(write_buf),
                                     I2C_MASTER_TIMEOUT_MS / portTICK_RATE_MS);

    return ret;
}

uint8_t Ltr329Als::readPartId() {
    uint8_t partId;
    ESP_ERROR_CHECK(registerReadByte(LTR329ALS01_REG_PART_ID, &partId));
    return partId;
}

uint8_t Ltr329Als::readManufacturerId() {
    uint8_t manufacturerId;
    ESP_ERROR_CHECK(registerReadByte(LTR329ALS01_REG_MANUFAC_ID, &manufacturerId));
    return manufacturerId;
}

void Ltr329Als::enable() {
    // Turn on
    ltr329als01_als_contr_value control;
    control.raw = 0;
    control.als_mode = 1;
    // control.sw_reset = 1;
    ESP_ERROR_CHECK(registerWriteByte(LTR329ALS01_REG_ALS_CONTR, control.raw));
}

void disable() {
    // TODO: Turn off
}

ltr329als01_status_value Ltr329Als::readStatus() {
    ltr329als01_status_value status;
    ESP_ERROR_CHECK(registerReadByte(LTR329ALS01_REG_ALS_STATUS, &(status.raw)));
    return status;
}

ltr329als01_als_data_value Ltr329Als::readChannel0() {
    ltr329als01_als_data_value data_ch0;
    ESP_ERROR_CHECK(registerReadByte(LTR329ALS01_REG_ALS_DATA_CH0_0, data_ch0.raw));
    ESP_ERROR_CHECK(registerReadByte(LTR329ALS01_REG_ALS_DATA_CH0_1, data_ch0.raw + 1));
    return data_ch0;
}

ltr329als01_als_data_value Ltr329Als::readChannel1() {
    ltr329als01_als_data_value data_ch1;
    ESP_ERROR_CHECK(registerReadByte(LTR329ALS01_REG_ALS_DATA_CH1_0, data_ch1.raw));
    ESP_ERROR_CHECK(registerReadByte(LTR329ALS01_REG_ALS_DATA_CH1_1, data_ch1.raw + 1));
    return data_ch1;
}
