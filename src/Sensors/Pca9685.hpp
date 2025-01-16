#pragma once

#include <cinttypes>
#include <esp_err.h>

// Register addresses
enum pca9685_reg_t {
    PCA9685_REG_MODE1 = 0x00,        // Mode Register 1
    PCA9685_REG_MODE2 = 0x01,        // Mode Register 2
    PCA9685_REG_SUBADR1 = 0x02,      // I2C-bus subaddress 1
    PCA9685_REG_SUBADR2 = 0x03,      // I2C-bus subaddress 2
    PCA9685_REG_SUBADR3 = 0x04,      // I2C-bus subaddress 3
    PCA9685_REG_ALLCALLADR = 0x05,   // LED All Call I2C-bus address
    PCA9685_REG_LED0_ON_L = 0x06,    // LED0 on tick, low byte
    PCA9685_REG_LED0_ON_H = 0x07,    // LED0 on tick, high byte
    PCA9685_REG_LED0_OFF_L = 0x08,   // LED0 off tick, low byte
    PCA9685_REG_LED0_OFF_H = 0x09,   // LED0 off tick, high byte
    PCA9685_REG_ALLLED_ON_L = 0xFA,  // Load all LEDn_ON registers, low
    PCA9685_REG_ALLLED_ON_H = 0xFB,  // Load all LEDn_ON registers, high
    PCA9685_REG_ALLLED_OFF_L = 0xFC, // Load all LEDn_OFF registers, low
    PCA9685_REG_ALLLED_OFF_H = 0xFD, // Load all LEDn_OFF registers, high
    PCA9685_REG_PRESCALE = 0xFE,     // Prescaler for PWM output frequency
    PCA9685_REG_TESTMODE = 0xFF,     // Test mode register
};

// MODE1 register bit fields
struct Pca9685Mode1Value {
    union {
        uint8_t raw;
        struct {
            uint8_t allCall : 1; // Respond to LED All Call I2C-bus address
            uint8_t sub3 : 1;    // Respond to I2C-bus subaddress 3
            uint8_t sub2 : 1;    // Respond to I2C-bus subaddress 2
            uint8_t sub1 : 1;    // Respond to I2C-bus subaddress 1
            uint8_t sleep : 1;   // Low power mode. Oscillator off
            uint8_t autoInc : 1; // Auto-Increment enabled
            uint8_t extClk : 1;  // Use EXTCLK pin clock
            uint8_t restart : 1; // Restart enabled
        };
    };
};

// MODE2 register bit fields
struct Pca9685Mode2Value {
    union {
        uint8_t raw;
        struct {
            uint8_t outNe : 2;    // Output enable settings
            uint8_t outDrv : 1;   // Output driver configuration
            uint8_t och : 1;      // Output change configuration
            uint8_t invrt : 1;    // Output logic state inverted
            uint8_t reserved : 3; // Reserved bits
        };
    };
};

class Pca9685 {
public:
    explicit Pca9685(int i2cNum);

    // Initialize the device with a specific PWM frequency
    auto init(float pwmFreq = 1000.0f) -> bool;

    // Reset the device
    void reset();

    // Put device into sleep mode (low power)
    void sleep();

    // Wake device from sleep mode
    void wakeup();

    // Set PWM frequency in Hz (24Hz to 1526Hz)
    void setPwmFreq(float freq);

    // Set PWM value for a channel (0-4095)
    auto setPwm(uint8_t channel, uint16_t value) -> bool;

    // Set PWM values for all channels at once
    auto setAllPwm(uint16_t value) -> bool;

private:
    static constexpr uint8_t PCA9685_I2C_ADDR = 0x40;
    static constexpr uint32_t OSC_FREQ = 25000000; // Internal oscillator = 25MHz
    static constexpr uint16_t MAX_PWM = 4095;      // 12-bit resolution

    int i2cNum_;
    float pwmFreq_;

    auto registerRead(pca9685_reg_t reg) -> uint8_t;
    void registerWrite(pca9685_reg_t reg, uint8_t value);
    auto setPwmValues(uint8_t channel, uint16_t on, uint16_t off) -> bool;
};
