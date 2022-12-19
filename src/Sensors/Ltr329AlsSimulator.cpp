#if SIMULATOR

#include <cstdio>

#include "Ltr329Als.hpp"
#include "sindarin-debug.h"

#define I2C_MASTER_TIMEOUT_MS 1000
#define LTR329ALS01_SENSOR_ADDR 0x29

struct ltr329als01_als_contr_value {
    // 000 Gain 1x (1 lux to 64k lux) (default)
    // 001 Gain 2x (0.5 lux to 32k lux)
    // 010 Gain 4x (0.25 lux to 16k lux)
    // 011 Gain 8x (0.125 lux to 8k lux)
    // 100 Reserved
    // 101 Reserved
    // 110 Gain 48x (0.02 lux to 1.3k lux)
    // 111 Gain 96x (0.01 lux to 600 lux)
    enum ALSGain {
        ALSGain0 = 0,
        ALSGain2 = 1,
        ALSGain4 = 2,
        ALSGain8 = 3,
        ALSGain48 = 6,
        ALSGain96 = 7,
    };
    union {
        uint8_t raw;
        // Least-significant bit first in a union
        struct {
            // 1 = Active mode, 0 = Stand-by mode
            uint8_t alsMode : 1;
            // 1 = Initial start-up procedure is started, bit has default value of 0 after start-up
            // 0 = Initial start-up procedure is NOT started (default)
            uint8_t swReset : 1;
            ALSGain alsGain : 3;
            // MUST write to 000 (default)
            uint8_t reserved : 3;
        };
    };
};

auto Ltr329Als::registerReadByte(ltr329als01_reg_t regAddr, uint8_t *data) -> esp_err_t {
    return ESP_OK;
}

auto Ltr329Als::registerWriteByte(ltr329als01_reg_t regAddr, uint8_t data) -> esp_err_t {
    return ESP_OK;
}

auto Ltr329Als::readPartId() -> uint8_t { return 0xA0; }

auto Ltr329Als::readManufacturerId() -> uint8_t { return 0x05; }

auto Ltr329Als::enable() -> bool {
    log_d("Enabling Ltr329Als");
    return true;
}

auto Ltr329Als::disable() -> bool {
    log_d("Disabling Ltr329Als");
    return true;
}

auto Ltr329Als::readStatus(ltr329als01_status_value &out) -> bool {
    out.alsDataStatus = 1;
    return true;
}

auto Ltr329Als::readData(uint16_t &channel1, uint16_t &channel0) -> bool {
    channel1 = 500;
    channel0 = 500;
    return true;
}

#endif
