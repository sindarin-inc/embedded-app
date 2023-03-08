/*
 * SPDX-FileCopyrightText: 2017-2022 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "Storage/include/EFusesCustomTable.h"

#include <assert.h>

// md5_digest_table d3f09607385401b808efed8b9e3c1553
// This file was generated from the file EFusesCustomTable.csv. DO NOT CHANGE THIS FILE MANUALLY.
// If you want to change some fields, you need to change EFusesCustomTable.csv file
// then run `efuse_common_table` or `efuse_custom_table` command it will generate this file.
// To show efuse_table run the command 'show_efuse_table'.

static const esp_efuse_desc_t USER_DATA_MODULE_VERSION[] = {
    {EFUSE_BLK3, 56, 4}, // Module version,
};

static const esp_efuse_desc_t USER_DATA_SERIAL_NUMBER[] = {
    {EFUSE_BLK3, 64, 112}, // Serial number,
};

const esp_efuse_desc_t *ESP_EFUSE_USER_DATA_MODULE_VERSION[] = {
    &USER_DATA_MODULE_VERSION[0], // Module version
    NULL};

const esp_efuse_desc_t *ESP_EFUSE_USER_DATA_SERIAL_NUMBER[] = {
    &USER_DATA_SERIAL_NUMBER[0], // Serial number
    NULL};
