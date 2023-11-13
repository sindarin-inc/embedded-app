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

struct Mmc5603Ctrl1Value {
    union {
        uint8_t raw;
        struct {
            // These bandwidth selection bits adjust the length of the decimation filter. They
            // control the duration of each measurement.
            // | BW1 | BW0 | Measurement Time |
            // |-----|-----|------------------|
            // | 0   | 0   | 6.6ms            |
            // | 0   | 1   | 3.5ms            |
            // | 1   | 0   | 2.0ms            |
            // | 1   | 1   | 1.2ms            |
            // Note: X/Y/Z channel measurements are taken sequentially. Delay Time among those
            // measurements is 1/3 of the Measurement Time defined in the table.
            uint8_t bw0 : 1;
            uint8_t bw1 : 1;
            // Writing "1" will disable this channel, and reduce Measurement Time and total charge
            // per measurement.When a channel is disabled it is simply skipped during Take Measure
            // routine. Its output register is not reset and will maintain the last value written to
            // it when this channel was active. Note: Y/Z needs to be inhibited the same time in
            // case needed.
            uint8_t xInhibit : 1;
            uint8_t yInhibit : 1;
            uint8_t zInhibit : 1;
            // Writing "1" into this location will bring a DC current through the self-test coil of
            // the sensor. This current will cause an offset of the magnetic field. This function is
            // used to check whether the sensor has been saturated.
            uint8_t stEnp : 1;
            // The function of this bit is similar to ST_ENP, but the offset of the magnetic field
            // is of opposite polarity.
            uint8_t stEnm : 1;
            // Software Reset. Writing "1" will cause the part to reset, similar to power-up. It
            // will clear all registers and also re-read OTP as part of its startup routine. The
            // power on time is 20mS.
            uint8_t swReset : 1;
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

auto Mmc5603::readProductId(uint8_t *productIdOut) -> esp_err_t {
    return registerReadByte(MMC5603_REG_PRODUCT_ID, productIdOut);
}

void Mmc5603::configureIfNeeded() {
    if (configured_) {
        return;
    }
    log_d("Configuring MMC5603");
    Mmc5603Ctrl1Value ctrl1Value;
    ctrl1Value.bw0 = 1;
    ctrl1Value.bw1 = 1;
    esp_err_t err = registerWriteByte(MMC5603_REG_CTRL_1, ctrl1Value.raw);
    if (err != ESP_OK) {
        log_e("Failed to write to MMC5603_REG_CTRL_1 (error: %s)", esp_err_to_name(err));
        return;
    }
    configured_ = true;
}

auto Mmc5603::waitForMeasurement() -> bool {
    int timeoutMs = 5;
    auto start = Millis();

    // Changing the magnetometer settings (especially bandwidth selection) can affect this.
    // Experimentally: I'm seeing:
    //  * bw0=0, bw1=0: ~10ms
    //  * bw0=1, bw0=1: ~1.8ms
    // But it seems that waiting 2ms isn't enough to reliably get a measurement ready.
    // So here it's better to wait a bit longer (more time for light sleep) than to get a
    // measurement right away.
    vTaskDelay(3000 / (1000000 / configTICK_RATE_HZ));

    // Wait for valid data
    Mmc5603StatusValue statusValue;
    statusValue.raw = 0;
    while ((Millis() - start) < timeoutMs) {
        esp_err_t err = registerReadByte(MMC5603_REG_STATUS_1, (uint8_t *)&statusValue.raw);
        if (err != ESP_OK) {
            log_e("Failed to read from MMC5603_REG_STATUS_1 (error: %s)", esp_err_to_name(err));
            return err;
        }
        if (statusValue.measMDone == 1) {
            return true;
            break;
        }
        Delay(1);
        log_w("Mag sensor not ready");
    }
    return false;
}

auto Mmc5603::readX20Bit() -> float {
    configureIfNeeded();

    Mmc5603Ctrl0Value ctrl0Value;
    ctrl0Value.autoSrEn = 1;
    ctrl0Value.takeMeasM = 1;
    ESP_ERROR_CHECK(registerWriteByte(MMC5603_REG_CTRL_0, ctrl0Value.raw));

    waitForMeasurement();

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
    configureIfNeeded();

    Mmc5603Ctrl0Value ctrl0Value;
    ctrl0Value.autoSrEn = 1;
    ctrl0Value.takeMeasM = 1;
    ESP_ERROR_CHECK(registerWriteByte(MMC5603_REG_CTRL_0, ctrl0Value.raw));

    waitForMeasurement();

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
    configureIfNeeded();

    Mmc5603Ctrl0Value ctrl0Value;
    ctrl0Value.autoSrEn = 1;
    ctrl0Value.takeMeasM = 1;
    esp_err_t err;
    err = registerWriteByte(MMC5603_REG_CTRL_0, ctrl0Value.raw);
    if (err != ESP_OK) {
        log_e("Failed to write to MMC5603_REG_CTRL_0 (error: %s)", esp_err_to_name(err));
        return err;
    }

    waitForMeasurement();

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
    configureIfNeeded();

    Mmc5603Ctrl0Value ctrl0Value;
    ctrl0Value.autoSrEn = 1;
    ctrl0Value.takeMeasM = 1;
    esp_err_t err = ESP_OK;
    err = registerWriteByte(MMC5603_REG_CTRL_0, ctrl0Value.raw);
    if (err != ESP_OK) {
        log_e("Failed to write to MMC5603_REG_CTRL_0 (error: %s)", esp_err_to_name(err));
        return err;
    }

    waitForMeasurement();

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

    return err;
}

auto Mmc5603::readTemperature(int &temp) -> esp_err_t {
    configureIfNeeded();

    Mmc5603Ctrl0Value ctrl0Value;
    ctrl0Value.autoSrEn = 1;
    ctrl0Value.takeMeasT = 1;
    esp_err_t err = ESP_OK;
    err = registerWriteByte(MMC5603_REG_CTRL_0, ctrl0Value.raw);
    if (err != ESP_OK) {
        log_e("Failed to write to MMC5603_REG_CTRL_0 (error: %s)", esp_err_to_name(err));
        return err;
    }
    uint8_t raw;

    // TODO: Wait for measurement

    err = registerReadByte(MMC5603_REG_TOUT, &raw);
    if (err != ESP_OK) {
        log_e("Failed to read from MMC5603_REG_TOUT (error: %s)", esp_err_to_name(err));
        return err;
    }

    // temp = (float)raw * 0.8f - 75.0f;
    temp = raw * 8 - 750;
    temp = temp / 10;

    return err;
}
