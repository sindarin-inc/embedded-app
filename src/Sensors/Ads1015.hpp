#pragma once

#include <cinttypes>
#include <esp_err.h>

// Register addresses
enum ads1015_reg_t {
    ADS1015_REG_CONVERSION = 0x00,
    ADS1015_REG_CONFIG = 0x01,
    ADS1015_REG_LO_THRESH = 0x02,
    ADS1015_REG_HI_THRESH = 0x03,
};

// Config register bit fields
struct Ads1015ConfigValue {
    union {
        uint16_t raw;
        struct {
            // Comparator queue and disable
            uint16_t compQueue : 2;
            // Latching comparator
            uint16_t compLat : 1;
            // Comparator polarity
            uint16_t compPol : 1;
            // Comparator mode
            uint16_t compMode : 1;
            // Data rate
            uint16_t dataRate : 3;
            // Mode (single/continuous)
            uint16_t mode : 1;
            // Programmable gain amplifier
            uint16_t pga : 3;
            // Multiplexer configuration
            uint16_t mux : 3;
            // Operational status
            uint16_t opStatus : 1;
        };
    };
};

// Gain settings in volts
enum class Ads1015Gain {
    GAIN_6_144V = 0, // +/-6.144V
    GAIN_4_096V = 1, // +/-4.096V
    GAIN_2_048V = 2, // +/-2.048V (default)
    GAIN_1_024V = 3, // +/-1.024V
    GAIN_0_512V = 4, // +/-0.512V
    GAIN_0_256V = 5  // +/-0.256V
};

class Ads1015 {
public:
    explicit Ads1015(int i2cNum);

    // Initialize the device
    auto init() -> bool;

    // Set the gain (voltage range)
    void setGain(Ads1015Gain gain);

    // Read from a single-ended input channel (0-3)
    auto readSingleEnded(uint8_t channel) -> int16_t;

    // Read differential between channels 0 and 1
    auto readDifferential_0_1() -> int16_t;

    // Read differential between channels 2 and 3
    auto readDifferential_2_3() -> int16_t;

    // Convert raw counts to voltage based on current gain setting
    auto computeVolts(int16_t counts) -> float;

private:
    static constexpr uint8_t ADS1015_I2C_ADDR = 0x48;
    static constexpr uint16_t ADS1015_CONV_READY = 0x8000;

    int i2cNum_;
    Ads1015Gain currentGain_ = Ads1015Gain::GAIN_2_048V;

    auto registerRead(ads1015_reg_t reg) -> uint16_t;
    void registerWrite(ads1015_reg_t reg, uint16_t value);
    auto startConversion(uint16_t mux) -> int16_t;
    auto waitForConversion() -> bool;
};
