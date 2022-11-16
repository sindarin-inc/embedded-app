#pragma once

#include <esp_err.h>
#include <stdlib.h>

/**
 * @brief  Retrieves address and size of coredump data in flash.
 *         This function is always available, even when core dump is disabled in menuconfig.
 *
 * @param  out_addr   pointer to store image address in flash.
 * @param  out_size   pointer to store image size in flash (including checksum). In bytes.
 *
 * @return ESP_OK on success, otherwise \see esp_err_t
 */
esp_err_t esp_core_dump_image_get(size_t* out_addr, size_t *out_size);

