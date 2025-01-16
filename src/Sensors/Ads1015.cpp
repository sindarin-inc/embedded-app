#include "ADS1015.hpp"

#include <driver/i2c.h>
#include <esp_log.h>

#include "sindarin-debug.h"

#define I2C_MASTER_TIMEOUT_MS 1000

Ads1015::Ads1015(int i2cNum) : i2cNum_(i2cNum) {}

auto Ads1015::init() -> bool {
    // Try to read the config register to verify device is responding
    uint16_t configReg = registerRead(ADS1015_REG_CONFIG);
    if (configReg == 0xFFFF) {
        log_e("Failed to communicate with ADS1015");
        return false;
    }

    // Set default configuration
    Ads1015ConfigValue config{};
    config.compQueue = 0b11; // Disable comparator
    config.dataRate = 0b100; // 1600 SPS
    config.mode = 1;         // Single-shot mode
    config.pga = static_cast<uint16_t>(currentGain_);
    config.mux = 0b100; // Single-ended AIN0

    registerWrite(ADS1015_REG_CONFIG, config.raw);
    return true;
}

void Ads1015::setGain(Ads1015Gain gain) { currentGain_ = gain; }

auto Ads1015::readSingleEnded(uint8_t channel) -> int16_t {
    if (channel > 3) {
        return 0;
    }

    // Configure mux for single-ended reading
    uint16_t mux = 0x4000 | ((channel & 0x03) << 12);
    return startConversion(mux);
}

auto Ads1015::readDifferential_0_1() -> int16_t {
    return startConversion(0x0000); // Differential P = AIN0, N = AIN1
}

auto Ads1015::readDifferential_2_3() -> int16_t {
    return startConversion(0x3000); // Differential P = AIN2, N = AIN3
}

auto Ads1015::computeVolts(int16_t counts) -> float {
    // Convert based on current gain setting
    float fsRange;
    switch (currentGain_) {
    case Ads1015Gain::GAIN_6_144V:
        fsRange = 6.144f;
        break;
    case Ads1015Gain::GAIN_4_096V:
        fsRange = 4.096f;
        break;
    case Ads1015Gain::GAIN_2_048V:
        fsRange = 2.048f;
        break;
    case Ads1015Gain::GAIN_1_024V:
        fsRange = 1.024f;
        break;
    case Ads1015Gain::GAIN_0_512V:
        fsRange = 0.512f;
        break;
    case Ads1015Gain::GAIN_0_256V:
        fsRange = 0.256f;
        break;
    default:
        return 0.0f;
    }
    return counts * (fsRange / 2048.0f); // ADS1015 is 12-bit
}

auto Ads1015::registerRead(ads1015_reg_t reg) -> uint16_t {
    uint8_t data[2];
    esp_err_t err = i2c_master_write_read_device(static_cast<i2c_port_t>(i2cNum_), ADS1015_I2C_ADDR,
                                                 reinterpret_cast<uint8_t *>(&reg), 1, data, 2,
                                                 I2C_MASTER_TIMEOUT_MS / portTICK_PERIOD_MS);

    if (err != ESP_OK) {
        log_e("Failed to read from ADS1015 register 0x%02x (error: %s)", reg, esp_err_to_name(err));
        return 0xFFFF;
    }

    return (data[0] << 8) | data[1];
}

void Ads1015::registerWrite(ads1015_reg_t reg, uint16_t value) {
    uint8_t data[3] = {static_cast<uint8_t>(reg), static_cast<uint8_t>(value >> 8),
                       static_cast<uint8_t>(value & 0xFF)};

    esp_err_t err =
        i2c_master_write_to_device(static_cast<i2c_port_t>(i2cNum_), ADS1015_I2C_ADDR, data,
                                   sizeof(data), I2C_MASTER_TIMEOUT_MS / portTICK_PERIOD_MS);

    if (err != ESP_OK) {
        log_e("Failed to write to ADS1015 register 0x%02x (error: %s)", reg, esp_err_to_name(err));
    }
}

auto Ads1015::startConversion(uint16_t mux) -> int16_t {
    // Configure for single conversion
    Ads1015ConfigValue config{};
    config.raw = registerRead(ADS1015_REG_CONFIG);
    config.mux = mux >> 12;
    config.pga = static_cast<uint16_t>(currentGain_);
    config.opStatus = 1; // Start conversion

    registerWrite(ADS1015_REG_CONFIG, config.raw);

    // Wait for conversion complete
    if (!waitForConversion()) {
        return 0;
    }

    // Read and convert value
    int16_t raw = static_cast<int16_t>(registerRead(ADS1015_REG_CONVERSION));
    return raw >> 4; // Convert 16-bit to 12-bit
}

auto Ads1015::waitForConversion() -> bool {
    // Maximum conversion time at 128 SPS is about 8ms
    const int maxRetries = 10;
    int retries = 0;

    while (retries < maxRetries) {
        uint16_t config = registerRead(ADS1015_REG_CONFIG);
        if (config & ADS1015_CONV_READY) {
            return true;
        }
        vTaskDelay(1);
        retries++;
    }

    log_e("ADS1015 conversion timeout");
    return false;
}
