#include <esp_idf_version.h>
#include <stdbool.h>
#include <stdint.h>
#include <ulp_riscv.h>
#include <ulp_riscv_utils.h>

#include "../components/ulp-i2c/src/ulp/ulp_riscv_i2c_ulp_core.h"
#include "MagDefs.h"
#include "UlpTableLookup.h"

volatile int open;
volatile int wakeupCount;
volatile time_t wakeupEpochTime;

void magReading() {
    // initiate a reading
    uint8_t dataRd = 0x21;
    uint8_t magReadingBytes[9];
    ulp_riscv_i2c_master_set_slave_reg_addr(MAG_CONTROL_0_REG);
    ulp_riscv_i2c_master_write_to_device(&dataRd, 1);

    ulp_riscv_delay_cycles(10 * ULP_RISCV_CYCLES_PER_MS);

    // keep reading status until measurement is ready
    ulp_riscv_i2c_master_set_slave_reg_addr(MAG_DEVICE_STATUS_REG_1);
    while (1) {
        ulp_riscv_i2c_master_read_from_device(&dataRd, 1);
        if ((dataRd & MAG_STATUS_MEAS_DONE_BIT) != 0) {
            break;
        }
        ulp_riscv_delay_cycles(1 * ULP_RISCV_CYCLES_PER_MS);
    }

    // read magnetometer data
    ulp_riscv_i2c_master_set_slave_reg_addr(0x00);
    ulp_riscv_i2c_master_read_from_device(magReadingBytes, 9);

    // TODO: use this measurement to decide if we should wake up
    uint8_t xReading = magReadingBytes[MAG_X_OUT_0];
    uint8_t zReading = magReadingBytes[MAG_Z_OUT_0];
    if (checkTable(xReading, zReading)) {
        open = 1;
        // wake up cpu
        ulp_riscv_wakeup_main_processor();
    }
}

void init() { open = 0; }

int main(void) {
    init();

    // if wakeup epoch time is unset, just stay asleep until woken from magnetometer
    // instead of periodically checking time.
    if (wakeupEpochTime != 0) {
        // check if we have run enough times to wake up the CPU for periodic checks
        if (wakeupCount <= 0) {
            // time to wake up the CPU
            ulp_riscv_wakeup_main_processor();
            return 0;
        }

        wakeupCount--;
    }

    // read magnetometer to see if the temple is open
    ulp_riscv_i2c_master_set_slave_addr(MAG_ADDRESS);
    magReading();
    return 0;
}
