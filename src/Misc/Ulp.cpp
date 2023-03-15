#include "Ulp.hpp"
#include "sindarin-debug.h"
#include "esp_sleep.h"
#include "ulp_riscv.h"
#include "ulp_riscv_i2c.h"
#include "../../ulp/mag_defs.h"

extern const uint8_t ulp_main_bin_start[] asm("_binary_ulp_main_bin_start");
extern const uint8_t ulp_main_bin_end[]   asm("_binary_ulp_main_bin_end");


static void init_ulp_program(void)
{
    esp_err_t err = ulp_riscv_load_binary(ulp_main_bin_start, (ulp_main_bin_end - ulp_main_bin_start));
    
    log_e("load binary err %d\n", err);
    ESP_ERROR_CHECK(err);

    /* The first argument is the period index, which is not used by the ULP-RISC-V timer
     * The second argument is the period in microseconds, which gives a wakeup time period of: 40ms
     */
    ulp_set_wakeup_period(0, 1000000);

    /* Start the program */
    err = ulp_riscv_run();
    log_e("run binary err %d\n", err);
    ESP_ERROR_CHECK(err);
}

static void init_i2c(void)
{
    ulp_riscv_i2c_cfg_t i2c_cfg;
    i2c_cfg.i2c_pin_cfg.sda_io_num = I2C_MASTER_SDA_IO;
    i2c_cfg.i2c_pin_cfg.scl_io_num = I2C_MASTER_SCL_IO;
    i2c_cfg.i2c_pin_cfg.sda_pullup_en = true;
    i2c_cfg.i2c_pin_cfg.scl_pullup_en = true;
    i2c_cfg.i2c_timing_cfg.scl_low_period = 5;
    i2c_cfg.i2c_timing_cfg.scl_high_period = 5;
    i2c_cfg.i2c_timing_cfg.sda_duty_period = 2;
    i2c_cfg.i2c_timing_cfg.scl_start_period = 3;
    i2c_cfg.i2c_timing_cfg.scl_stop_period = 6;
    i2c_cfg.i2c_timing_cfg.i2c_trans_timeout = 20;
    ulp_riscv_i2c_master_init(&i2c_cfg);
}

void Ulp::DeepSleepWithMagWakeup() {
    init_i2c();
    log_i("entering deep sleep, wake up with magnetometer");

    init_ulp_program();
    esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_PERIPH, ESP_PD_OPTION_ON);
    ESP_ERROR_CHECK(esp_sleep_enable_ulp_wakeup());
    esp_deep_sleep_start();
}