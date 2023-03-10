//______________________________________________________________________________
//------------------------------------------------------------------------------
/*!
    \file       WiFiESP.hpp

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
#include <esp_wifi.h>
#include <functional>
#include <stdint.h>
#include <string>

//-----------------------------------------------------------------------------
// Defines
//-----------------------------------------------------------------------------

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

// Define Scanning Processes
#define WIFI_SCAN_RUNNING (-1)
#define WIFI_SCAN_FAILED (-2)

// Wifi STA Status Type
typedef enum {
    WIFIESP_STATUS_NO_WIFI = 255, // for compatibility with WiFi Shield library
    WIFIESP_STATUS_IDLE_STATUS = 0,
    WIFIESP_STATUS_NO_SSID_AVAIL = 1,
    WIFIESP_STATUS_SCAN_COMPLETED = 2,
    WIFIESP_STATUS_CONNECTED = 3,
    WIFIESP_STATUS_CONNECT_FAILED = 4,
    WIFIESP_STATUS_CONNECTION_LOST = 5,
    WIFIESP_STATUS_DISCONNECTED = 6
} wifiesp_status_t;

// Define WPA2 Authentication Methods
typedef enum {
    WIFIESP_WPA2_AUTH_TLS = 0,
    WIFIESP_WPA2_AUTH_PEAP = 1,
    WIFIESP_WPA2_AUTH_TTLS = 2
} wifiesp_wpa2_auth_method_t;

// Wifi Event IDs
typedef enum {
    WIFIESP_EVENT_WIFI_READY = 0,
    WIFIESP_EVENT_WIFI_SCAN_DONE,
    WIFIESP_EVENT_WIFI_STA_START,
    WIFIESP_EVENT_WIFI_STA_STOP,
    WIFIESP_EVENT_WIFI_STA_CONNECTED,
    WIFIESP_EVENT_WIFI_STA_DISCONNECTED,
    WIFIESP_EVENT_WIFI_STA_AUTHMODE_CHANGE,
    WIFIESP_EVENT_WIFI_STA_GOT_IP,
    WIFIESP_EVENT_WIFI_STA_GOT_IP6,
    WIFIESP_EVENT_WIFI_STA_LOST_IP,
    WIFIESP_EVENT_WIFI_AP_START,
    WIFIESP_EVENT_WIFI_AP_STOP,
    WIFIESP_EVENT_WIFI_AP_STACONNECTED,
    WIFIESP_EVENT_WIFI_AP_STADISCONNECTED,
    WIFIESP_EVENT_WIFI_AP_STAIPASSIGNED,
    WIFIESP_EVENT_WIFI_AP_PROBEREQRECVED,
    WIFIESP_EVENT_WIFI_AP_GOT_IP6,
    WIFIESP_EVENT_WIFI_FTM_REPORT,
    WIFIESP_EVENT_ETH_START,
    WIFIESP_EVENT_ETH_STOP,
    WIFIESP_EVENT_ETH_CONNECTED,
    WIFIESP_EVENT_ETH_DISCONNECTED,
    WIFIESP_EVENT_ETH_GOT_IP,
    WIFIESP_EVENT_ETH_GOT_IP6,
    WIFIESP_EVENT_WPS_ER_SUCCESS,
    WIFIESP_EVENT_WPS_ER_FAILED,
    WIFIESP_EVENT_WPS_ER_TIMEOUT,
    WIFIESP_EVENT_WPS_ER_PIN,
    WIFIESP_EVENT_WPS_ER_PBC_OVERLAP,
    WIFIESP_EVENT_SC_SCAN_DONE,
    WIFIESP_EVENT_SC_FOUND_CHANNEL,
    WIFIESP_EVENT_SC_GOT_SSID_PSWD,
    WIFIESP_EVENT_SC_SEND_ACK_DONE,
    WIFIESP_EVENT_PROV_INIT,
    WIFIESP_EVENT_PROV_DEINIT,
    WIFIESP_EVENT_PROV_START,
    WIFIESP_EVENT_PROV_END,
    WIFIESP_EVENT_PROV_CRED_RECV,
    WIFIESP_EVENT_PROV_CRED_FAIL,
    WIFIESP_EVENT_PROV_CRED_SUCCESS,
    WIFIESP_EVENT_MAX
} wifiesp_event_id_t;

// create our own ip address type
typedef esp_ip4_addr_t wifiesp_ip4address;

// Const zero ip address
const wifiesp_ip4address IPADDRESS_NONE = {.addr = 0};
