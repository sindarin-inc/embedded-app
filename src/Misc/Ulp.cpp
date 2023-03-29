#include "Ulp.hpp"

#include <cstddef>
#include <cstdint>
#include <driver/rtc_io.h>
#include <esp_idf_version.h>
#include <esp_sleep.h>
#include <ulp_riscv.h>
#include <ulp_riscv_i2c.h>

#include "Misc/Platform.hpp"
#include "Ulp/MagDefs.h"
#include "sindarin-debug.h"

#define ULP_TIMED_WAKEUP_INTERVAL_SECONDS 60
#define ULP_RUN_INTERVAL_MS 1000

extern const uint8_t ULP_MAIN_BIN_START[] asm("_binary_ulp_src_bin_start");
extern const uint8_t ULP_MAIN_BIN_END[] asm("_binary_ulp_src_bin_end");

extern uint32_t ulp_wakeupCount;   // NOLINT(readability-identifier-naming)
extern uint32_t ulp_open;          // NOLINT(readability-identifier-naming)
extern time_t ulp_wakeupEpochTime; // NOLINT(readability-identifier-naming)

void UlpWakeupCheck(SolClock &clock) {
#if SIMULATOR
    log_i("UlpWakeupCheck");
#else
    esp_sleep_wakeup_cause_t cause = esp_sleep_get_wakeup_cause();
    if (cause == ESP_SLEEP_WAKEUP_ULP) {
        log_i("CPU woke from ULP");
        log_i("open %" PRIu32 " wake %lld", ulp_open, ulp_wakeupEpochTime);

        if (ulp_open > 0) {
            // woke because temples are open
            log_i("ULP detected temple open");

            // continue with regular startup
            return;
        }

        uint8_t hours, minutes;
        clock.getClockTime(hours, minutes);
        time_t timeNow = clock.getEpochTime();
        log_i("time is now: %02d:%02d %lld", hours, minutes, clock.getEpochTime());

        if (timeNow >= ulp_wakeupEpochTime) {
            // reached the specified wakeup time
            // do timed check here
            // return if it's time to fully wake up

            // for now, just go back to sleep for another 2 minutes
            ulp_wakeupEpochTime = timeNow + (static_cast<long long>(2 * 60));
        }

        // back to sleep
        log_i("going back to sleep");
        UlpDeepSleepMagnetometerWakeupWithWakeTime(ulp_wakeupEpochTime);
    }

#endif
}

void UlpDeepSleepMagnetometerWakeup() { UlpDeepSleepMagnetometerWakeupWithWakeTime((time_t)0); }

void UlpDeepSleepMagnetometerWakeupWithWakeTime(time_t wakeTime) {
#if SIMULATOR
    log_i("UlpDeepSleepMagnetometerWakeupWithWakeTime");
#else

    rtc_gpio_init((gpio_num_t)I2C_MASTER_SDA_IO);
    rtc_gpio_init((gpio_num_t)I2C_MASTER_SCL_IO);

    ulp_riscv_i2c_cfg_t i2cCfg;
    i2cCfg.i2c_pin_cfg.sda_io_num = I2C_MASTER_SDA_IO;
    i2cCfg.i2c_pin_cfg.scl_io_num = I2C_MASTER_SCL_IO;
    i2cCfg.i2c_pin_cfg.sda_pullup_en = true;
    i2cCfg.i2c_pin_cfg.scl_pullup_en = true;
    i2cCfg.i2c_timing_cfg.scl_low_period = 5;
    i2cCfg.i2c_timing_cfg.scl_high_period = 5;
    i2cCfg.i2c_timing_cfg.sda_duty_period = 2;
    i2cCfg.i2c_timing_cfg.scl_start_period = 3;
    i2cCfg.i2c_timing_cfg.scl_stop_period = 6;
    i2cCfg.i2c_timing_cfg.i2c_trans_timeout = 20;
    ulp_riscv_i2c_master_init(&i2cCfg);

    log_i("entering deep sleep, wake up with magnetometer");
    esp_err_t err =
        ulp_riscv_load_binary(ULP_MAIN_BIN_START, (ULP_MAIN_BIN_END - ULP_MAIN_BIN_START));
    if (err != ESP_OK) {
        log_e("load binary err %d", err);
        return;
    }

    /* The first argument is the period index, which is not used by the ULP-RISC-V timer
     * The second argument is the period in microseconds, which gives a wakeup time period of: 40ms
     */
    ulp_set_wakeup_period(0, ULP_RUN_INTERVAL_MS * 1000);

    // set the interval to wake up to do periodic checks
    ulp_wakeupCount = (ULP_TIMED_WAKEUP_INTERVAL_SECONDS * 1000) / ULP_RUN_INTERVAL_MS;
    ulp_wakeupEpochTime = wakeTime;

    /* Start the program */
    err = ulp_riscv_run();
    if (err != ESP_OK) {
        log_e("run binary err %d", err);
        return;
    }

    esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_PERIPH, ESP_PD_OPTION_ON);
    err = esp_sleep_enable_ulp_wakeup();
    if (err != ESP_OK) {
        log_e("error enabling ulp wakeup from deep sleep %d", err);
        return;
    }
    esp_deep_sleep_start();
#endif
}
