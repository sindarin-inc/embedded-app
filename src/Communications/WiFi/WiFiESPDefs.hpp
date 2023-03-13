//______________________________________________________________________________
//------------------------------------------------------------------------------
/*!
    \file       WiFiESPDefs.hpp

    \brief      WiFi ESP32 Wireless driver Defines

    Copyright (c) 2023 Sindarin, Inc. - All Right Reserved

    The code contained in this file is confidential and may only be used
    under license and/or written consent of Sindarin.
*/
//______________________________________________________________________________
//------------------------------------------------------------------------------

#pragma once

//-----------------------------------------------------------------------------
// Includes
//-----------------------------------------------------------------------------
#include "esp_wifi.h"

//------------------------------------------------------------------------------
// Defines
//------------------------------------------------------------------------------
// Define the WiFi power type
typedef enum {
    WIFI_POWER_19_5dBm = 78,   // 19.5dBm
    WIFI_POWER_19dBm = 76,     // 19dBm
    WIFI_POWER_18_5dBm = 74,   // 18.5dBm
    WIFI_POWER_17dBm = 68,     // 17dBm
    WIFI_POWER_15dBm = 60,     // 15dBm
    WIFI_POWER_13dBm = 52,     // 13dBm
    WIFI_POWER_11dBm = 44,     // 11dBm
    WIFI_POWER_8_5dBm = 34,    // 8.5dBm
    WIFI_POWER_7dBm = 28,      // 7dBm
    WIFI_POWER_5dBm = 20,      // 5dBm
    WIFI_POWER_2dBm = 8,       // 2dBm
    WIFI_POWER_MINUS_1dBm = -4 // -1dBm
} wifi_power_t;

// Wifi STA Status Type
typedef enum {
    WIFIESP_STATUS_IDLE_STATUS = 0,
    WIFIESP_STATUS_NO_SSID_AVAIL,
    WIFIESP_STATUS_SCAN_INPROGRESS,
    WIFIESP_STATUS_SCAN_COMPLETED,
    WIFIESP_STATUS_CONNECTED,
    WIFIESP_STATUS_CONNECT_FAILED,
    WIFIESP_STATUS_CONNECTION_LOST,
    WIFIESP_STATUS_DISCONNECTED
} wifiesp_status_t;

// Define Scanning Processes
#define WIFI_SCAN_RUNNING (-1)
#define WIFI_SCAN_FAILED (-2)

// create our own ip address type
typedef esp_ip4_addr_t wifiesp_ip4address;

// Const zero ip address
const wifiesp_ip4address IPADDRESS_NONE = {.addr = 0};

/*
config WIFIESP_DEFAULT_SCAN_LIST_SIZE
        int "Max size of scan list"
        range 0 20
        default 10
        help
                The size of array that will be used to retrieve the list of access points.
*/
#define WIFIESP_DEFAULT_SCAN_LIST_SIZE 10
