#pragma once

#include <cinttypes>
#include <esp_err.h>

enum ltr329als01_reg_t {
    // ALS operation mode control SW reset (R/W)
    LTR329ALS01_REG_ALS_CONTR = 0x80,
    // ALS measurement rate in active mode (R/W)
    LTR329ALS01_REG_ALS_MEAS_RATE = 0x85,
    // Part Number ID and Revision ID (R)
    LTR329ALS01_REG_PART_ID = 0x86,
    // Manufacturer ID (R)
    LTR329ALS01_REG_MANUFAC_ID = 0x87,
    // ALS measurement CH1 data, lower byte (R)
    LTR329ALS01_REG_ALS_DATA_CH1_0 = 0x88,
    // ALS measurement CH1 data, upper byte (R)
    LTR329ALS01_REG_ALS_DATA_CH1_1 = 0x89,
    // ALS measurement CH0 data, lower byte (R)
    LTR329ALS01_REG_ALS_DATA_CH0_0 = 0x8A,
    // ALS measurement CH0 data, upper byte (R)
    LTR329ALS01_REG_ALS_DATA_CH0_1 = 0x8B,
    // ALS new data status
    LTR329ALS01_REG_ALS_STATUS = 0x8C,
    // Interrupt settings (R/W)
    LTR329ALS01_REG_ALS_INTERRUPT = 0x8F,
    // ALS interrupt upper threshold, lower byte (R/W)
    LTR329ALS01_REG_ALS_THRES_UP_0 = 0x97,
    // ALS interrupt upper threshold, upper byte (R/W)
    LTR329ALS01_REG_ALS_THRES_UP_1 = 0x98,
    // ALS interrupt lower threshold, lower byte (R/W)
    LTR329ALS01_REG_ALS_THRES_LOW_0 = 0x99,
    // ALS interrupt lower threshold, upper byte (R/W)
    LTR329ALS01_REG_ALS_THRES_LOW_1 = 0x9A,
    // ALS interrupt persist setting (R/W)
    LTR329ALS01_REG_INTERRUPT_PERSIST = 0x9E,
};

struct Ltr329als01StatusValue {
    union {
        uint8_t raw;
        struct {
            uint8_t reserved : 2;
            // 0 = OLD data (data already read), (default)
            // 1 = NEW data (first time data is read)
            uint8_t alsDataStatus : 1;
            // 0 = Interrupt signal INACTIVE (default)
            // 1 = Interrupt signal ACTIVE
            uint8_t alsInterruptStatus : 1;
            // 000 = ALS measured data in Gain 1X (default)
            // 001 = ALS measured data in Gain 2X
            // 010 = ALS measured data in Gain 4X
            // 011 = ALS measured data in Gain 8X
            // 100 = Invalid
            // 101 = Invalid
            // 110 = ALS measured data in Gain 48X
            // 111 = ALS measured data in Gain 96X
            uint8_t alsDataGainRange : 3;
            // 0 = ALS Data is Valid (default)
            // 1 = ALS Data is Invalid
            uint8_t alsDataValid : 1;
        };
    };

    auto isValid() -> bool { return alsDataStatus == 1 && alsDataValid == 0; }
};

struct Ltr329als01AlsDataValue {
    union {
        uint8_t raw[2];
        uint16_t value;
    };
};

class Ltr329Als {
public:
    Ltr329Als(int i2cNum) : i2cNum_(i2cNum) {}

    auto readPartId() -> uint8_t;
    auto readManufacturerId() -> uint8_t;

    auto enable() -> bool;
    auto disable() -> bool;

    auto readStatus(Ltr329als01StatusValue &out) -> bool;

    // From the datasheet:
    //   The ALS_DATA registers should be read as a group, with the lower address read back first
    //   (i.e. read 0x88 first, then read 0x89). These two registers should also be read before
    //   reading channel-0 data (from registers 0x8A, 0x8B). When the I2C read operation starts, all
    //   four ALS data registers are locked until the I2C read operation of register 0x8B is
    //   completed.
    // The datasheet isn't especially direct about the differences in these two registers, but there
    // is one graph that shows that the channel 1 register handles higher wavelengths and the
    // channel 0 register handles lower wavelengths.
    // At 48x gain (110)
    //  69 /  59 John's bedroom with the light on
    // 603 / 493 Outdoor light on an overcast day
    // 700 / 500 Our office on a sunny day
    auto readData(uint16_t &channel1, uint16_t &channel0) -> bool;

private:
    int i2cNum_;

    auto registerReadByte(ltr329als01_reg_t regAddr, uint8_t *data) -> esp_err_t;

    auto registerWriteByte(ltr329als01_reg_t regAddr, uint8_t data) -> esp_err_t;
};
