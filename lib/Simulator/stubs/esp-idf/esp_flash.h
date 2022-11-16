#pragma once

#include <esp_err.h>
#include <stdint.h>

/** @brief Read data from the SPI flash chip
 *
 * @param chip Pointer to identify flash chip. Must have been successfully initialised via esp_flash_init()
 * @param buffer Pointer to a buffer where the data will be read. To get better performance, this should be in the DRAM and word aligned.
 * @param address Address on flash to read from. Must be less than chip->size field.
 * @param length Length (in bytes) of data to read.
 *
 * There are no alignment constraints on buffer, address or length.
 *
 * @note If on-chip flash encryption is used, this function returns raw (ie encrypted) data. Use the flash cache
 * to transparently decrypt data.
 *
 * @return
 *      - ESP_OK: success
 *      - ESP_ERR_NO_MEM: Buffer is in external PSRAM which cannot be concurrently accessed, and a temporary internal buffer could not be allocated.
 *      - or a flash error code if operation failed.
 */
esp_err_t esp_flash_read(void *chip, void *buffer, uint32_t address, uint32_t length);