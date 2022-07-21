#include "Mmc5603.hpp"

#include <driver/i2c.h>
#include <esp_log.h>

#define I2C_MASTER_TIMEOUT_MS 1000
#define MMC5603_SENSOR_ADDR 0x30

typedef struct {
    union {
        uint8_t raw;
        struct {
            // Factory use only, reset value is 0.
            uint8_t meas_m_done_int : 1;
            // Factory use only, reset value is 0.
            uint8_t meas_t_done_int : 1;
            // Factory use only, reset value is 0.
            uint8_t mtd_flag_int : 1;
            // Factory use only, reset value is 0.
            uint8_t st_fail : 1;
            // This bit is an indicator of successfully reading its OTP memory either as part of its
            // power up sequence, or after an I2C command that reloads the OTP memory, such as
            // resetting the chip and refreshing the OTP registers.
            uint8_t otp_read_done : 1;
            // This bit is an indicator of the selftest signal, it keeps low once the device PASS
            // selftest.
            uint8_t sat_sensor : 1;
            // This bit indicates that a measurement of magnetic field is done and the data is ready
            // to be read. This bit is reset only when any of the magnetic data registers is read.
            uint8_t meas_m_done : 1;
            // This bit indicates that a measurement of temperature is done and the data is ready to
            // be read. This bit is reset only when the temperature register is read.
            uint8_t meas_t_done : 1;
        };
    };
} mmc5603_status_value;

typedef struct {
    union {
        uint8_t raw;
        struct {
            // Take Measure of Magnetic field, or TM_M bit. Writing a 1 into this location causes
            // the chip to perform a magnetic measurement. This bit is self-clearing at the end of
            // each measurement.
            uint8_t take_meas_m : 1;
            // Take Measure of Temperature, or TM_T bit. Writing a 1 into this location causes the
            // chip to perform a temperature measurement. This bit is self-clearing at the end of
            // each measurement.
            uint8_t take_meas_t : 1;
            // Factory use only, reset value is 0.
            uint8_t start_mdt : 1;
            // Writing a 1 into this location will cause the chip to do the Set operation, which
            // will allow large set current to flow through the sensor coils for 375ns. This bit is
            // self-cleared at the end of Set operation.
            uint8_t do_set : 1;
            // Writing a 1 into this location will cause the chip to do the Reset operation, which
            // will allow large reset current to flow through the sensor coils for 375ns. This bit
            // is self-cleared at the end of Reset operation.
            uint8_t do_reset : 1;
            // Writing a 1 into this location will enable the function of automatic set/reset. This
            // function applies to both on-demand and continuous-time measurements. This bit must be
            // set to 1 in order to activate the feature of periodic set. This bit is recommended to
            // set to “1” in the application.
            uint8_t auto_sr_en : 1;
            // Writing a 1 into this location will enable the function of automatic self-test. The
            // threshold in register 1EH, 1FH, 20H should be set before this bit is set to 1. This
            // bit clears itself after the operation is completed.
            uint8_t auto_st_en : 1;
            // Writing a 1 into this location will start the calculation of the measurement period
            // according to the ODR. This bit should be set before continuous-mode measurements are
            // started. This bit is self- cleared after the measurement period is calculated by
            // internal circuits.
            uint8_t cmm_freq_en : 1;
        };
    };
} mmc5603_ctrl_0_value;

esp_err_t Mmc5603::registerReadByte(mmc5603_reg_t reg_addr, uint8_t *data) {
    return i2c_master_write_read_device(i2cNum_, MMC5603_SENSOR_ADDR, (uint8_t *)&reg_addr, 1, data,
                                        1, I2C_MASTER_TIMEOUT_MS / portTICK_RATE_MS);
}

esp_err_t Mmc5603::registerWriteByte(mmc5603_reg_t reg_addr, uint8_t data) {
    int ret;
    uint8_t write_buf[2] = {reg_addr, data};
    ret = i2c_master_write_to_device(i2cNum_, MMC5603_SENSOR_ADDR, write_buf, sizeof(write_buf),
                                     I2C_MASTER_TIMEOUT_MS / portTICK_RATE_MS);

    return ret;
}

uint8_t Mmc5603::readProductId() {
    uint8_t productId;
    registerReadByte(MMC5603_REG_PRODUCT_ID, &productId);
    return productId;
}

float Mmc5603::readX20Bit() {
    mmc5603_ctrl_0_value ctrl0Value;
    ctrl0Value.auto_sr_en = 1;
    ctrl0Value.take_meas_m = 1;
    ESP_ERROR_CHECK(registerWriteByte(MMC5603_REG_CTRL_0, ctrl0Value.raw));

    uint32_t xInt = 0;
    float xValue = 0;
    uint8_t data;
    ESP_ERROR_CHECK(registerReadByte(MMC5603_REG_XOUT_0, &data));
    xInt |= data << 12;
    ESP_ERROR_CHECK(registerReadByte(MMC5603_REG_XOUT_1, &data));
    xInt |= data << 4;
    ESP_ERROR_CHECK(registerReadByte(MMC5603_REG_XOUT_2, &data));
    xInt |= (data >> 4);
    xValue = ((float)xInt - 524288.0) / 16384.0f;
    return xValue;
}

float Mmc5603::readX() {
    mmc5603_ctrl_0_value ctrl0Value;
    ctrl0Value.auto_sr_en = 1;
    ctrl0Value.take_meas_m = 1;
    ESP_ERROR_CHECK(registerWriteByte(MMC5603_REG_CTRL_0, ctrl0Value.raw));

    // 16 bit mode
    uint32_t xInt = 0;
    float xValue = 0;
    uint8_t data;
    ESP_ERROR_CHECK(registerReadByte(MMC5603_REG_XOUT_0, &data));
    xInt |= data << 8;
    ESP_ERROR_CHECK(registerReadByte(MMC5603_REG_XOUT_1, &data));
    xInt |= data;
    xValue = ((float)xInt - 32768.0) / 1024.0f;
    return xValue;
}
