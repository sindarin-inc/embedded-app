/*
 * SPDX-FileCopyrightText: 2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */
#if !SIMULATOR
#include "rtc_wake_stub_example.h"

#include <driver/i2c.h>
#include <esp_log.h>
#include <inttypes.h>
#include <math.h>

#include "../Sensors/TempleCloseSensor.hpp"
#include "esp_sleep.h"
#include "sdkconfig.h"
#define log_i(format, ...) ESP_LOGI("Sindarin", format, ##__VA_ARGS__);
/*
enum mmc5603_reg_t {
    MMC5603_REG_XOUT_0 = 0x00,
    MMC5603_REG_XOUT_1 = 0x01,
    MMC5603_REG_YOUT_0 = 0x02,
    MMC5603_REG_YOUT_1 = 0x03,
    MMC5603_REG_ZOUT_0 = 0x04,
    MMC5603_REG_ZOUT_1 = 0x05,
    MMC5603_REG_XOUT_2 = 0x06,
    MMC5603_REG_YOUT_2 = 0x07,
    MMC5603_REG_ZOUT_2 = 0x08,
    MMC5603_REG_TOUT = 0x09,
    MMC5603_REG_STATUS_1 = 0x18,
    MMC5603_REG_ODR = 0x1A,
    MMC5603_REG_CTRL_0 = 0x1B,
    MMC5603_REG_CTRL_1 = 0x1C,
    MMC5603_REG_CTRL_2 = 0x1D,
    MMC5603_REG_ST_X_TH = 0x1E,
    MMC5603_REG_ST_Y_TH = 0x1F,
    MMC5603_REG_ST_Z_TH = 0x20,
    MMC5603_REG_ST_X = 0x27,
    MMC5603_REG_ST_Y = 0x28,
    MMC5603_REG_ST_Z = 0x29,
    MMC5603_REG_PRODUCT_ID = 0x39,
};

struct Xyz8bit {
    int8_t x;
    int8_t y;
    int8_t z;
};

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

esp_err_t mmc5603_registerReadByte(enum mmc5603_reg_t regAddr, uint8_t *data) {
    return i2c_master_write_read_device(0, MMC5603_SENSOR_ADDR, (uint8_t *)&regAddr, 1, data, 1,
                                        I2C_MASTER_TIMEOUT_MS / portTICK_RATE_MS);
}

esp_err_t mmc5603_registerWriteByte(enum mmc5603_reg_t regAddr, uint8_t data) {
    int ret;
    uint8_t writeBuf[2] = {(uint8_t)(regAddr), data};
    ret = i2c_master_write_to_device(0, MMC5603_SENSOR_ADDR, writeBuf, sizeof(writeBuf),
                                     I2C_MASTER_TIMEOUT_MS / portTICK_RATE_MS);

    return ret;
}

uint8_t mmc5603_readProductId() {
    uint8_t productId;
    mmc5603_registerReadByte(MMC5603_REG_PRODUCT_ID, &productId);
    return productId;
}

float mmc5603_readX20Bit() {
    struct Mmc5603Ctrl0Value ctrl0Value;
    ctrl0Value.autoSrEn = 1;
    ctrl0Value.takeMeasM = 1;
    ESP_ERROR_CHECK(mmc5603_registerWriteByte(MMC5603_REG_CTRL_0, ctrl0Value.raw));

    uint32_t xInt = 0;
    float xValue = 0;
    uint8_t data;
    ESP_ERROR_CHECK(mmc5603_registerReadByte(MMC5603_REG_XOUT_0, &data));
    xInt |= data << 12;
    ESP_ERROR_CHECK(mmc5603_registerReadByte(MMC5603_REG_XOUT_1, &data));
    xInt |= data << 4;
    ESP_ERROR_CHECK(mmc5603_registerReadByte(MMC5603_REG_XOUT_2, &data));
    xInt |= (data >> 4);
    // Offset by half the range of a 20-bit unsigned number (2^19 = 524288)
    // and divide by the range -30G - +30G (524288 / 30) to get the float value.
    xValue = ((float)xInt - 524288.0) / 17476.267f;
    return xValue;
}

float mmc5603_readX() {
    struct Mmc5603Ctrl0Value ctrl0Value;
    ctrl0Value.autoSrEn = 1;
    ctrl0Value.takeMeasM = 1;
    ESP_ERROR_CHECK(mmc5603_registerWriteByte(MMC5603_REG_CTRL_0, ctrl0Value.raw));

    // 16 bit mode
    uint32_t xInt = 0;
    float xValue = 0;
    uint8_t data;
    ESP_ERROR_CHECK(mmc5603_registerReadByte(MMC5603_REG_XOUT_0, &data));
    xInt |= data << 8;
    ESP_ERROR_CHECK(mmc5603_registerReadByte(MMC5603_REG_XOUT_1, &data));
    xInt |= data;
    // Offset by half the range of a 16-bit unsigned number (2^15 = 32768)
    // and divide by the range -30G - +30G (32768 / 30) to get the float value.
    xValue = ((float)xInt - 32768.0) / 1092.267f;
    return xValue;
}

bool mmc5603_readXYZ8Bit(struct Xyz8bit *out) {
    struct Mmc5603Ctrl0Value ctrl0Value;
    ctrl0Value.autoSrEn = 1;
    ctrl0Value.takeMeasM = 1;
    esp_err_t err;
    err = mmc5603_registerWriteByte(MMC5603_REG_CTRL_0, ctrl0Value.raw);
    if (err != ESP_OK) {
        // log_e("Failed to write to MMC5603_REG_CTRL_0 (error: %s)", esp_err_to_name(err));
        return false;
    }
    err = mmc5603_registerReadByte(MMC5603_REG_XOUT_0, (uint8_t *)&(out->x));
    if (err != ESP_OK) {
        // log_e("Failed to read from MMC5603_REG_XOUT_0 (error: %s)", esp_err_to_name(err));
        return false;
    }
    out->x -= 128;
    err = mmc5603_registerReadByte(MMC5603_REG_YOUT_0, (uint8_t *)&(out->y));
    if (err != ESP_OK) {
        // log_e("Failed to read from MMC5603_REG_YOUT_0 (error: %s)", esp_err_to_name(err));
        return false;
    }
    out->y -= 128;
    err = mmc5603_registerReadByte(MMC5603_REG_ZOUT_0, (uint8_t *)&(out->z));
    if (err != ESP_OK) {
        // log_e("Failed to read from MMC5603_REG_ZOUT_0 (error: %s)", esp_err_to_name(err));
        return false;
    }
    out->z -= 128;
    return true;
}

bool mmc5603_readXZ8Bit(struct Xyz8bit *out) {
    struct Mmc5603Ctrl0Value ctrl0Value;
    ctrl0Value.autoSrEn = 1;
    ctrl0Value.takeMeasM = 1;
    esp_err_t err;
    err = mmc5603_registerWriteByte(MMC5603_REG_CTRL_0, ctrl0Value.raw);
    if (err != ESP_OK) {
        // log_e("Failed to write to MMC5603_REG_CTRL_0 (error: %s)", esp_err_to_name(err));
        return false;
    }
    err = mmc5603_registerReadByte(MMC5603_REG_XOUT_0, (uint8_t *)&(out->x));
    if (err != ESP_OK) {
        // log_e("Failed to read from MMC5603_REG_XOUT_0 (error: %s)", esp_err_to_name(err));
        return false;
    }
    out->x -= 128;
    err = mmc5603_registerReadByte(MMC5603_REG_ZOUT_0, (uint8_t *)&(out->z));
    if (err != ESP_OK) {
        // log_e("Failed to read from MMC5603_REG_ZOUT_0 (error: %s)", esp_err_to_name(err));
        return false;
    }
    out->z -= 128;
    return true;
}
*/
/*
 * Deep sleep wake stub function is a piece of code that will be loaded into 'RTC Fast Memory'.
 * The first way is to use the RTC_IRAM_ATTR attribute to place a function into RTC memory,
 * The second way is to place the function into any source file whose name starts with
 * rtc_wake_stub. Files names rtc_wake_stub* have their contents automatically put into RTC memory
 * by the linker.
 *
 * First, call esp_set_deep_sleep_wake_stub to set the wake stub function as the RTC stub entry,
 * The wake stub function runs immediately as soon as the chip wakes up - before any normal
 * initialisation, bootloader, or ESP-IDF code has run. After the wake stub runs, the SoC
 * can go back to sleep or continue to start ESP-IDF normally.
 *
 * Wake stub code must be carefully written, there are some rules for wake stub:
 * 1) The wake stub code can only access data loaded in RTC memory.
 * 2) The wake stub code can only call functions implemented in ROM or loaded into RTC Fast Memory.
 * 3) RTC memory must include any read-only data (.rodata) used by the wake stub.
 */

// counter value, stored in RTC memory
static RTC_IRAM_ATTR uint32_t s_count = 0;
static RTC_IRAM_ATTR const uint32_t s_max_count = 20;

// wakeup_cause stored in RTC memory
static RTC_IRAM_ATTR uint32_t wakeup_cause;

// wake up stub function stored in RTC memory
extern "C" void RTC_IRAM_ATTR esp_wake_deep_sleep /*wake_stub_example*/ (void) {

    // First thing -- if the temple is opened, don't do anything else
    // and just boot.
    /*
    struct Xyz8bit magReading;
    bool success = mmc5603_readXZ8Bit(&magReading);
    */
    log_i("Alive again!");
    /*
      if (!success) {
          log_i("Failed to read magnetometer");
      }
      log_i("attempting math");
      float angle = atan2f(magReading.x, magReading.z) * 180.0f / 3.14159;
      // log_d("Mag: %d %d %d %fº (success=%d)", magReading.x, magReading.y, magReading.z, angle,
      //       success);
      // Experimentally I've seen the following ranges:
      // -17º (closed) to 104º (open) (midpoint = 43.5º)
      // -150º (closed) to -73º (open) (midpoint = -112º)
      // -197 (closed) to -86 (open) (midpoint = -141.5º) - SOL01224400004
      // Handle either magnet polarity:
      if (angle < -45) {
          angle += 180;
      } else if (angle > 135) {
          angle -= 180;
      }
      // Define an angle, above which we will declare the temple as open and below which we will
      // declare it as closed. This value was discovered experimentally.
      const int templeMidpointAngle = 70;
      if (angle > templeMidpointAngle) {
          log_i("Temples open!");
          esp_default_wake_deep_sleep();
          return;
      }

      */
    TempleCloseSensor templeCloseSensor;

    templeCloseSensor.checkTemple();

    if (!templeCloseSensor.getOpened()) {
        log_i("Temples closed coolio!");
    } else {
        log_i("Temples open no need to go back to sleep!");
        return;
    }

    // Get wakeup cause.
    wakeup_cause = esp_sleep_get_wakeup_cause();
    // Increment the counter.
    s_count++;
    // Print the counter value and wakeup cause.
    // ESP_RTC_LOGI("wake stub: wakeup count is %d, wakeup cause is %d", s_count, wakeup_cause);

    if (s_count >= s_max_count) {
        // Reset s_count
        s_count = 0;
        log_i("Slept enough, waking up anyway");
        // Set the default wake stub.
        // There is a default version of this function provided in esp-idf.
        // esp_default_wake_deep_sleep();

        // Return from the wake stub function to continue
        // booting the firmware.
        return;
    }
    // s_count is < s_max_count, go back to deep sleep.

    // Set wakeup time in stub, if need to check GPIOs or read some sensor periodically in the
    // stub.
    esp_sleep_enable_timer_wakeup(1000000);

    // Print status.
    // ESP_RTC_LOGI("wake stub: going to deep sleep");
    log_i("Back to sleep");
    // Set stub entry, then going to deep sleep again.
    // esp_set_deep_sleep_wake_stub(&wake_stub_example);
    esp_deep_sleep_start();
}

#endif