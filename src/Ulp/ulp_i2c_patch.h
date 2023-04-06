#pragma once

void ulp_riscv_i2c_master_read_from_device_patch(uint8_t *data_rd, size_t size);
void ulp_riscv_i2c_master_write_to_device_patch(uint8_t *data_wr, size_t size);