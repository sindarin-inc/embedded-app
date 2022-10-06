#if !SIMULATOR

#include <driver/i2c.h>
#include <driver/ledc.h>
#include <driver/uart.h>
#include <esp_adc_cal.h>
#include <esp_err.h>
#include <soc/rtc.h>

#include "Misc/StringUtil.hpp"
#include "Platform.hpp"
#include "RTOS.h"
#include "Storage/include/EFusesCustomTable.h"
#include "config.h"
#include "sindarin-debug.h"

#if ULP_BACKLIGHT
#include "Misc/ULP.hpp"
#endif

#define BACKLIGHT_PWM_CHANNEL 0

esp_adc_cal_characteristics_t adcCalCharacteristics;

void InitializePlatform() {
    auto adcCalValue = esp_adc_cal_characterize(ADC_UNIT_1, ADC_ATTEN_DB_2_5, ADC_WIDTH_BIT_12,
                                                DEFAULT_VREF, &adcCalCharacteristics);
    if (adcCalValue != ESP_ADC_CAL_VAL_EFUSE_TP_FIT) {
        // Generally, we expect the Two Point calibration to be used.
        // It's unclear if we'll ever hit this warning but putting it in to be safe.
        log_w("Using ADC calibration type: %d", (int)adcCalValue);
    }
    adc1_config_width(ADC_WIDTH_BIT_12);
    // Attenuation of 2.5db gets us up to a max of 1250mV
    // Using our 442k/160k divider, that's 4.7V max
    adc1_config_channel_atten(BATT_CHANNEL_LEFT, ADC_ATTEN_DB_2_5);
    adc1_config_channel_atten(BATT_CHANNEL_RIGHT, ADC_ATTEN_DB_2_5);

    digitalWrite(RED_LED, LED_OFF);
    digitalWrite(GREEN_LED, LED_OFF);
    pinMode(RED_LED, OUTPUT);
    pinMode(GREEN_LED, OUTPUT);
#ifdef BLUE_LED
    digitalWrite(BLUE_LED, LED_OFF);
    pinMode(BLUE_LED, OUTPUT);
#endif
#ifdef SYS_ON
    // For now, we'll never have the MCU keep itself on. This should be similar to the behavior of
    // the RevX boards -- when the temples are closed, power will be cut off.
    digitalWrite(SYS_ON, LOW);
    pinMode(SYS_ON, OUTPUT);
#endif
#if USE_I2C
    // Initialize I2C port 0
    int i2cPort = 0;
    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = I2C_MASTER_SDA_IO,
        .scl_io_num = I2C_MASTER_SCL_IO,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
    };
    conf.master.clk_speed = I2C_MASTER_FREQ_HZ;
    i2c_param_config(i2cPort, &conf);
    i2c_driver_install(i2cPort, conf.mode, 0, 0, 0);
#endif

#if EINKET013TT1
#ifdef DISPLAY_POWER
    pinMode(DISPLAY_POWER, OUTPUT);
#endif
#if LEDC_BACKLIGHT
    // Backlight configs
    ledc_timer_config_t ledcTimer = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .duty_resolution = LEDC_TIMER_8_BIT,
        .timer_num = LEDC_TIMER_0,
        .freq_hz = 1000,
        .clk_cfg = LEDC_USE_RTC8M_CLK //
    };
    ledc_timer_config(&ledcTimer);

    ledc_channel_config_t ledcLChannel = {
        .gpio_num = BACKLIGHT_LEFT,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel = LEDC_CHANNEL_0,
        .intr_type = LEDC_INTR_DISABLE,
        .timer_sel = LEDC_TIMER_0,
        .duty = 0,
        .hpoint = 0 //
    };

    ledc_channel_config_t ledcRChannel = {
        .gpio_num = BACKLIGHT_RIGHT,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel = LEDC_CHANNEL_1,
        .intr_type = LEDC_INTR_DISABLE,
        .timer_sel = LEDC_TIMER_0,
        .duty = 0,
        .hpoint = 0 //
    };

    ledc_channel_config(&ledcLChannel);
    ledc_channel_config(&ledcRChannel);

    // This allows LEDC to work in light sleep on ESP32-S3
    esp_sleep_pd_config(ESP_PD_DOMAIN_RTC8M, ESP_PD_OPTION_ON);
#endif
#if ULP_BACKLIGHT
    UlpStart();
#endif
#endif // EINKET013TT1

#ifdef USB_CONNECTED_PIN
    pinMode(USB_CONNECTED_PIN, INPUT);
#endif
}

// Forked from TinyPico's GetBatteryVoltage() method:
// https://github.com/tinypico/tinypico-arduino/blob/master/TinyPICO-Helper/src/TinyPICO.cpp#L207
auto GetBatteryVoltageLeft() -> float {
    uint32_t raw = adc1_get_raw(BATT_CHANNEL_LEFT);
    uint32_t millivolts = esp_adc_cal_raw_to_voltage(raw, &adcCalCharacteristics);
    auto mv = (float)(UPPER_DIVIDER + LOWER_DIVIDER) / LOWER_DIVIDER * millivolts;
    return (float)mv / 1000.0;
}

auto GetBatteryVoltageRight() -> float {
    uint32_t raw = adc1_get_raw(BATT_CHANNEL_RIGHT);
    uint32_t millivolts = esp_adc_cal_raw_to_voltage(raw, &adcCalCharacteristics);
    auto mv = (float)(UPPER_DIVIDER + LOWER_DIVIDER) / LOWER_DIVIDER * millivolts;
    return (float)mv / 1000.0;
}

void SetCpuSpeedMhz(int speedMhz) {
    // This is a no-op if we're using automatic power management
#if DISABLE_POWER_MANAGEMENT
    if (speedMhz == esp_clk_cpu_freq() / 1000000) {
        return;
    }
    if (setCpuFrequencyMhz(speedMhz) != true) {
        log_e("Failed to setCpuFrequencyMhz(%d)", speedMhz);
    }
#endif
}

void SetBacklight(int percent) {
    auto dutyCycle = percent * 255 / 100;
    log_d("Setting backlight to %d%% (%d)", percent, dutyCycle);
#if LEDC_BACKLIGHT
    ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, dutyCycle);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);
    ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_1, dutyCycle);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_1);
#endif
#if ULP_BACKLIGHT
    UlpSetBrightness(dutyCycle);
#endif
}

void SetRightBackLight(int percent) {
#if LEDC_BACKLIGHT
    auto dutyCycle = percent * 255 / 100;
    ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_1, dutyCycle);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_1);
#endif
}

void SetLeftBackLight(int percent) {
#if LEDC_BACKLIGHT
    auto dutyCycle = percent * 255 / 100;
    ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, dutyCycle);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);
#endif
}

auto GetRunTimeMs() -> uint32_t { return FreeRTOS::getTimeSinceStart(); }

auto USBConnected() -> bool {
#ifdef USB_CONNECTED_PIN
    return digitalRead(USB_CONNECTED_PIN);
#else
    // No way to know if USB is connected on old boards. But it's probably not.
    return false;
#endif
}

auto GetSerialNumber() -> std::string {
    // Probably should read at boot to make logs more useful
    uint8_t serialNumber[8] = {0};
    ESP_ERROR_CHECK(esp_efuse_read_field_blob(ESP_EFUSE_USER_DATA_SERIAL_NUMBER, &serialNumber,
                                              sizeof(serialNumber) * 8));
    auto serialNumberHexString = StringFormat(
        "%02x%02x%02x%02x%02x%02x%02x%02x", serialNumber[0], serialNumber[1], serialNumber[2],
        serialNumber[3], serialNumber[4], serialNumber[5], serialNumber[6], serialNumber[7]);
    return serialNumberHexString;
}

#endif
