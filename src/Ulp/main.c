#include <esp_idf_version.h>
#include <stdbool.h>
#include <stdint.h>
#include <ulp_riscv.h>
#include <ulp_riscv_utils.h>
// Note: These must be below ulp_riscv.h and ulp_riscv_utils.h to avoid compiler warnings
#include <soc/rtc_cntl_reg.h>
#include <soc/soc.h>
#include <soc/soc_ulp.h>

#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 1, 0)
#include <ulp_riscv_i2c_ulp_core.h>
#else
#include "../components/ulp-i2c/src/ulp/ulp_riscv_i2c_ulp_core.h"
#endif
#include "MagDefs.h"
#include "UlpTableLookup.h"

// After the inactivity timer is triggered we explicitly want to wait until the temples are closed
// again before waking
volatile int waitForClose;

// Read magnetometer to see if the temple is open, wake the main CPU if so
void magReading() {
    ulp_riscv_i2c_master_set_slave_addr(MAG_ADDRESS);

    // initiate a reading
    // ctrl0Value.autoSrEn = 1;
    // ctrl0Value.takeMeasM = 1;
    // If we remove the autoSrEn here, and only run set periodically, then we might be able to
    // significantly reduce power consumption since that seems to drive 20mA spikes
    uint8_t dataRd = 0x21;
    uint8_t magReadingBytes[9];

    ulp_riscv_i2c_master_set_slave_reg_addr(MAG_CONTROL_0_REG);

    ulp_riscv_i2c_master_write_to_device(&dataRd, 1);

    // TODO: How is this determined? Is it necessary?
    ulp_riscv_delay_cycles(10 * ULP_RISCV_CYCLES_PER_MS);

    ulp_riscv_i2c_master_set_slave_reg_addr(MAG_DEVICE_STATUS_REG_1);

    // Keep reading status until measurement is ready with a 50ms timeout
    uint32_t start = ULP_RISCV_GET_CCOUNT();
    uint32_t end = start + 50 * ULP_RISCV_CYCLES_PER_MS;
    while (1) {
        ulp_riscv_i2c_master_read_from_device(&dataRd, 1);
        if ((dataRd & MAG_STATUS_MEAS_DONE_BIT) != 0) {
            break;
        }
        ulp_riscv_delay_cycles(1 * ULP_RISCV_CYCLES_PER_MS);

        if (ULP_RISCV_GET_CCOUNT() > end) {
            // TODO: Maybe consider turning back on if we get too many timeouts
            return;
        }
    }

    // read magnetometer data
    ulp_riscv_i2c_master_set_slave_reg_addr(0x00);

    // we can maybe read less data here by setting specific addresses perhaps
    // or just reading 5 bytes instead
    ulp_riscv_i2c_master_read_from_device(magReadingBytes, 9);

    uint8_t xReading = magReadingBytes[MAG_X_OUT_0];
    uint8_t zReading = magReadingBytes[MAG_Z_OUT_0];
    bool open = checkTable(xReading, zReading);

    if (open) {
        if (!waitForClose) {
            // wake up cpu
            ulp_riscv_wakeup_main_processor();
        }
    } else if (waitForClose) {
        waitForClose = 0;
    }
}

int main(void) {
    magReading();

    // The following is backported from ulp_riscv_halt in the latest IDF5 master (81e1e65995) since
    // `ulp_riscv_halt` causes sporadic crashes in 5.0.1. See also:
    // https://github.com/espressif/esp-idf/commit/099f6486861bee9365c33876879dff515e3a9f99

    // Setting the delay time after RISCV recv `DONE` signal, Ensure that action `RESET` can be
    // executed in time.
    REG_SET_FIELD(RTC_CNTL_COCPU_CTRL_REG, RTC_CNTL_COCPU_SHUT_2_CLK_DIS, 0x3F);

    // Suspends the ulp operation and reset the ULP core. Must be the final operation before going
    // to halt.
    SET_PERI_REG_MASK(RTC_CNTL_COCPU_CTRL_REG, RTC_CNTL_COCPU_DONE | RTC_CNTL_COCPU_SHUT_RESET_EN);

    while (1) {
    };

    return 0;
}
