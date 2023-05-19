#include "Mmc5603.hpp"

#include <driver/i2c.h>
#include <esp_log.h>
#include <freertos/FreeRTOS.h>

#include "sindarin-debug.h"

#define I2C_MASTER_TIMEOUT_MS 1000
#define MMC5603_SENSOR_ADDR 0x30

struct Mmc5603StatusValue {
    union {
        uint8_t raw;
        struct {
            // Factory use only, reset value is 0.
            uint8_t measMDoneInt : 1;
            // Factory use only, reset value is 0.
            uint8_t measTDoneInt : 1;
            // Factory use only, reset value is 0.
            uint8_t mtdFlagInt : 1;
            // Factory use only, reset value is 0.
            uint8_t stFail : 1;
            // This bit is an indicator of successfully reading its OTP memory either as part of its
            // power up sequence, or after an I2C command that reloads the OTP memory, such as
            // resetting the chip and refreshing the OTP registers.
            uint8_t otpReadDone : 1;
            // This bit is an indicator of the selftest signal, it keeps low once the device PASS
            // selftest.
            uint8_t satSensor : 1;
            // This bit indicates that a measurement of magnetic field is done and the data is ready
            // to be read. This bit is reset only when any of the magnetic data registers is read.
            uint8_t measMDone : 1;
            // This bit indicates that a measurement of temperature is done and the data is ready to
            // be read. This bit is reset only when the temperature register is read.
            uint8_t measTDone : 1;
        };
    };
};

struct Mmc5603Ctrl0Value {
    union {
        uint8_t raw;
        struct {
            // Take Measure of Magnetic field, or TM_M bit. Writing a 1 into this location causes
            // the chip to perform a magnetic measurement. This bit is self-clearing at the end of
            // each measurement.
            uint8_t takeMeasM : 1;
            // Take Measure of Temperature, or TM_T bit. Writing a 1 into this location causes the
            // chip to perform a temperature measurement. This bit is self-clearing at the end of
            // each measurement.
            uint8_t takeMeasT : 1;
            // Factory use only, reset value is 0.
            uint8_t startMdt : 1;
            // Writing a 1 into this location will cause the chip to do the Set operation, which
            // will allow large set current to flow through the sensor coils for 375ns. This bit is
            // self-cleared at the end of Set operation.
            uint8_t doSet : 1;
            // Writing a 1 into this location will cause the chip to do the Reset operation, which
            // will allow large reset current to flow through the sensor coils for 375ns. This bit
            // is self-cleared at the end of Reset operation.
            uint8_t doReset : 1;
            // Writing a 1 into this location will enable the function of automatic set/reset. This
            // function applies to both on-demand and continuous-time measurements. This bit must be
            // set to 1 in order to activate the feature of periodic set. This bit is recommended to
            // set to “1” in the application.
            uint8_t autoSrEn : 1;
            // Writing a 1 into this location will enable the function of automatic self-test. The
            // threshold in register 1EH, 1FH, 20H should be set before this bit is set to 1. This
            // bit clears itself after the operation is completed.
            uint8_t autoStEn : 1;
            // Writing a 1 into this location will start the calculation of the measurement period
            // according to the ODR. This bit should be set before continuous-mode measurements are
            // started. This bit is self- cleared after the measurement period is calculated by
            // internal circuits.
            uint8_t cmmFreqEn : 1;
        };
    };
};

auto Mmc5603::registerReadByte(mmc5603_reg_t regAddr, uint8_t *data) -> esp_err_t {
    return i2c_master_write_read_device((i2c_port_t)i2cNum_, MMC5603_SENSOR_ADDR,
                                        (uint8_t *)&regAddr, 1, data, 1,
                                        I2C_MASTER_TIMEOUT_MS / portTICK_RATE_MS);
}

auto Mmc5603::registerWriteByte(mmc5603_reg_t regAddr, uint8_t data) -> esp_err_t {
    int ret;
    uint8_t writeBuf[2] = {static_cast<uint8_t>(regAddr), data};
    ret = i2c_master_write_to_device((i2c_port_t)i2cNum_, MMC5603_SENSOR_ADDR, writeBuf,
                                     sizeof(writeBuf), I2C_MASTER_TIMEOUT_MS / portTICK_RATE_MS);

    return ret;
}

auto Mmc5603::readProductId() -> uint8_t {
    uint8_t productId;
    registerReadByte(MMC5603_REG_PRODUCT_ID, &productId);
    return productId;
}

auto Mmc5603::readX20Bit() -> float {
    Mmc5603Ctrl0Value ctrl0Value;
    ctrl0Value.autoSrEn = 1;
    ctrl0Value.takeMeasM = 1;
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
    // Offset by half the range of a 20-bit unsigned number (2^19 = 524288)
    // and divide by the range -30G - +30G (524288 / 30) to get the float value.
    xValue = ((float)xInt - 524288.0) / 17476.267f;
    return xValue;
}

auto Mmc5603::readX() -> float {
    Mmc5603Ctrl0Value ctrl0Value;
    ctrl0Value.autoSrEn = 1;
    ctrl0Value.takeMeasM = 1;
    ESP_ERROR_CHECK(registerWriteByte(MMC5603_REG_CTRL_0, ctrl0Value.raw));

    // 16 bit mode
    uint32_t xInt = 0;
    float xValue = 0;
    uint8_t data;
    ESP_ERROR_CHECK(registerReadByte(MMC5603_REG_XOUT_0, &data));
    xInt |= data << 8;
    ESP_ERROR_CHECK(registerReadByte(MMC5603_REG_XOUT_1, &data));
    xInt |= data;
    // Offset by half the range of a 16-bit unsigned number (2^15 = 32768)
    // and divide by the range -30G - +30G (32768 / 30) to get the float value.
    xValue = ((float)xInt - 32768.0) / 1092.267f;
    return xValue;
}

auto Mmc5603::readXYZ8Bit(Xyz8bit &out) -> esp_err_t {
    Mmc5603Ctrl0Value ctrl0Value;
    ctrl0Value.autoSrEn = 1;
    ctrl0Value.takeMeasM = 1;
    esp_err_t err;
    err = registerWriteByte(MMC5603_REG_CTRL_0, ctrl0Value.raw);
    if (err != ESP_OK) {
        log_e("Failed to write to MMC5603_REG_CTRL_0 (error: %s)", esp_err_to_name(err));
        return err;
    }
    err = registerReadByte(MMC5603_REG_XOUT_0, (uint8_t *)&(out.x));
    if (err != ESP_OK) {
        log_e("Failed to read from MMC5603_REG_XOUT_0 (error: %s)", esp_err_to_name(err));
        return err;
    }
    out.x -= 128;
    err = registerReadByte(MMC5603_REG_YOUT_0, (uint8_t *)&(out.y));
    if (err != ESP_OK) {
        log_e("Failed to read from MMC5603_REG_YOUT_0 (error: %s)", esp_err_to_name(err));
        return err;
    }
    out.y -= 128;
    err = registerReadByte(MMC5603_REG_ZOUT_0, (uint8_t *)&(out.z));
    if (err != ESP_OK) {
        log_e("Failed to read from MMC5603_REG_ZOUT_0 (error: %s)", esp_err_to_name(err));
        return err;
    }
    out.z -= 128;
    return err;
}

auto Mmc5603::readXZ8Bit(Xyz8bit &out) -> esp_err_t {
    Mmc5603Ctrl0Value ctrl0Value;
    ctrl0Value.autoSrEn = 1;
    ctrl0Value.takeMeasM = 1;
    ctrl0Value.takeMeasT = 1;
    esp_err_t err = ESP_OK;
    err = registerWriteByte(MMC5603_REG_CTRL_0, ctrl0Value.raw);
    if (err != ESP_OK) {
        log_e("Failed to write to MMC5603_REG_CTRL_0 (error: %s)", esp_err_to_name(err));
        return err;
    }
    err = registerReadByte(MMC5603_REG_XOUT_0, (uint8_t *)&(out.x));
    if (err != ESP_OK) {
        log_e("Failed to read from MMC5603_REG_XOUT_0 (error: %s)", esp_err_to_name(err));
        return err;
    }
    out.x -= 128;
    err = registerReadByte(MMC5603_REG_ZOUT_0, (uint8_t *)&(out.z));
    if (err != ESP_OK) {
        log_e("Failed to read from MMC5603_REG_ZOUT_0 (error: %s)", esp_err_to_name(err));
        return err;
    }
    out.z -= 128;

    // Also grab temp while we're here
    uint8_t raw;
    err = registerReadByte(MMC5603_REG_TOUT, &raw);
    if (err != ESP_OK) {
        log_e("Failed to read from MMC5603_REG_TOUT (error: %s)", esp_err_to_name(err));
        return err;
    }
    temperature_ = (float)raw * 0.8f - 75.0f;

    return err;
}
