#include "Pca9685.hpp"

#include <driver/i2c.h>
#include <esp_log.h>

#include "sindarin-debug.h"

#define I2C_MASTER_TIMEOUT_MS 1000

Pca9685::Pca9685(int i2cNum) : i2cNum_(i2cNum) {}

auto Pca9685::init(float pwmFreq) -> bool {
    // Reset the device first
    reset();
    vTaskDelay(pdMS_TO_TICKS(10)); // Wait for reset to complete

    // Set default mode
    Pca9685Mode1Value mode1{};
    mode1.autoInc = 1; // Enable auto-increment
    registerWrite(PCA9685_REG_MODE1, mode1.raw);

    // Set PWM frequency
    setPwmFreq(pwmFreq);

    return true;
}

void Pca9685::reset() {
    Pca9685Mode1Value mode1{};
    mode1.restart = 1;
    registerWrite(PCA9685_REG_MODE1, mode1.raw);
}

void Pca9685::sleep() {
    Pca9685Mode1Value mode1{};
    mode1.raw = registerRead(PCA9685_REG_MODE1);
    mode1.sleep = 1;
    registerWrite(PCA9685_REG_MODE1, mode1.raw);
    vTaskDelay(pdMS_TO_TICKS(5)); // Wait for oscillator to stop
}

void Pca9685::wakeup() {
    Pca9685Mode1Value mode1{};
    mode1.raw = registerRead(PCA9685_REG_MODE1);
    mode1.sleep = 0;
    registerWrite(PCA9685_REG_MODE1, mode1.raw);
    vTaskDelay(pdMS_TO_TICKS(5)); // Wait for oscillator to start
}

void Pca9685::setPwmFreq(float freq) {
    // Constrain frequency
    freq = std::max(24.0f, std::min(1526.0f, freq));
    pwmFreq_ = freq;

    float prescaleVal = ((OSC_FREQ / (freq * 4096.0f)) + 0.5f) - 1;
    uint8_t prescale = static_cast<uint8_t>(prescaleVal);

    // To change prescale, we must put device to sleep
    Pca9685Mode1Value oldMode{};
    oldMode.raw = registerRead(PCA9685_REG_MODE1);

    Pca9685Mode1Value sleepMode = oldMode;
    sleepMode.sleep = 1;
    registerWrite(PCA9685_REG_MODE1, sleepMode.raw);

    // Set prescale
    registerWrite(PCA9685_REG_PRESCALE, prescale);

    // Restore old mode
    registerWrite(PCA9685_REG_MODE1, oldMode.raw);

    vTaskDelay(pdMS_TO_TICKS(5));

    // Set restart bit
    oldMode.restart = 1;
    registerWrite(PCA9685_REG_MODE1, oldMode.raw);
}

auto Pca9685::setPwm(uint8_t channel, uint16_t value) -> bool {
    if (channel > 15) {
        return false;
    }

    // Constrain value to 12-bit
    value = std::min(value, MAX_PWM);

    return setPwmValues(channel, 0, value);
}

auto Pca9685::setAllPwm(uint16_t value) -> bool {
    // Constrain value to 12-bit
    value = std::min(value, MAX_PWM);

    uint8_t data[] = {
        static_cast<uint8_t>(PCA9685_REG_ALLLED_ON_L),
        0,
        0,                                         // ON time = 0
        static_cast<uint8_t>(value & 0xFF),        // OFF time low byte
        static_cast<uint8_t>((value >> 8) & 0x0F), // OFF time high byte
    };

    esp_err_t err =
        i2c_master_write_to_device(static_cast<i2c_port_t>(i2cNum_), PCA9685_I2C_ADDR, data,
                                   sizeof(data), I2C_MASTER_TIMEOUT_MS / portTICK_PERIOD_MS);

    if (err != ESP_OK) {
        log_e("Failed to write all PWM values (error: %s)", esp_err_to_name(err));
        return false;
    }

    return true;
}

auto Pca9685::registerRead(pca9685_reg_t reg) -> uint8_t {
    uint8_t value;
    esp_err_t err = i2c_master_write_read_device(static_cast<i2c_port_t>(i2cNum_), PCA9685_I2C_ADDR,
                                                 reinterpret_cast<uint8_t *>(&reg), 1, &value, 1,
                                                 I2C_MASTER_TIMEOUT_MS / portTICK_PERIOD_MS);

    if (err != ESP_OK) {
        log_e("Failed to read from PCA9685 register 0x%02x (error: %s)", reg, esp_err_to_name(err));
        return 0;
    }

    return value;
}

void Pca9685::registerWrite(pca9685_reg_t reg, uint8_t value) {
    uint8_t data[] = {static_cast<uint8_t>(reg), value};

    esp_err_t err =
        i2c_master_write_to_device(static_cast<i2c_port_t>(i2cNum_), PCA9685_I2C_ADDR, data,
                                   sizeof(data), I2C_MASTER_TIMEOUT_MS / portTICK_PERIOD_MS);

    if (err != ESP_OK) {
        log_e("Failed to write to PCA9685 register 0x%02x (error: %s)", reg, esp_err_to_name(err));
    }
}

auto Pca9685::setPwmValues(uint8_t channel, uint16_t on, uint16_t off) -> bool {
    uint8_t data[] = {
        static_cast<uint8_t>(PCA9685_REG_LED0_ON_L + 4 * channel),
        static_cast<uint8_t>(on & 0xFF),        // ON time low byte
        static_cast<uint8_t>((on >> 8) & 0x0F), // ON time high byte
        static_cast<uint8_t>(off & 0xFF),       // OFF time low byte
        static_cast<uint8_t>((off >> 8) & 0x0F) // OFF time high byte
    };

    esp_err_t err =
        i2c_master_write_to_device(static_cast<i2c_port_t>(i2cNum_), PCA9685_I2C_ADDR, data,
                                   sizeof(data), I2C_MASTER_TIMEOUT_MS / portTICK_PERIOD_MS);

    if (err != ESP_OK) {
        log_e("Failed to write PWM values for channel %d (error: %s)", channel,
              esp_err_to_name(err));
        return false;
    }

    return true;
}
