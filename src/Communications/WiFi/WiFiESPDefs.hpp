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

#include "IPAddress.h"
#include "esp_smartconfig.h"
#include "wifi_provisioning/manager.h"

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

// Wifi Event Information
typedef union {
    wifi_event_sta_scan_done_t wifiScanDone;
    wifi_event_sta_authmode_change_t wifiStaAuthmodeChange;
    wifi_event_sta_connected_t wifiStaConnected;
    wifi_event_sta_disconnected_t wifiStaDisconnected;
    wifi_event_sta_wps_er_pin_t wpsErPin;
    wifi_event_sta_wps_fail_reason_t wpsFailReason;
    wifi_event_ap_probe_req_rx_t wifiApProbereqrecved;
    wifi_event_ap_staconnected_t wifiApStaconnected;
    wifi_event_ap_stadisconnected_t wifiApStadisconnected;
    wifi_event_ftm_report_t wifiFtmReport;
    ip_event_ap_staipassigned_t wifiApStaipassigned;
    ip_event_got_ip_t gotIp;
    ip_event_got_ip6_t gotIp6;
    smartconfig_event_got_ssid_pswd_t scGotSsidPswd;
    esp_eth_handle_t ethConnected;
    wifi_sta_config_t provCredRecv;
    wifi_prov_sta_fail_reason_t provFailReason;
} wifiesp_event_info_t;

// Wifi Event Type
typedef struct {
    wifiesp_event_id_t eventId;
    wifiesp_event_info_t eventInfo;
} wifiesp_event_t;

// Define Event Callback function pointer
typedef void (*wifiesp_event_cb_t)(wifiesp_event_id_t event);
typedef std::function<void(wifiesp_event_id_t event, wifiesp_event_info_t info)>
    wifiesp_event_func_cb_t;
typedef void (*wifiesp_event_cb_sys_t)(wifiesp_event_t *event);

// Define wifi event id type for the callback list
typedef size_t wifi_event_id_t;

// Define the Event Status Bits
static const int WIFIESP_EVENTSTATUSBIT_AP_STARTED = BIT0;
static const int WIFIESP_EVENTSTATUSBIT_AP_HAS_IP6 = BIT1;
static const int WIFIESP_EVENTSTATUSBIT_AP_HAS_CLIENT = BIT2;
static const int WIFIESP_EVENTSTATUSBIT_STA_STARTED = BIT3;
static const int WIFIESP_EVENTSTATUSBIT_STA_CONNECTED = BIT4;
static const int WIFIESP_EVENTSTATUSBIT_STA_HAS_IP = BIT5;
static const int WIFIESP_EVENTSTATUSBIT_STA_HAS_IP6 = BIT6;
static const int WIFIESP_EVENTSTATUSBIT_ETH_STARTED = BIT7;
static const int WIFIESP_EVENTSTATUSBIT_ETH_CONNECTED = BIT8;
static const int WIFIESP_EVENTSTATUSBIT_ETH_HAS_IP = BIT9;
static const int WIFIESP_EVENTSTATUSBIT_ETH_HAS_IP6 = BIT10;
static const int WIFIESP_EVENTSTATUSBIT_WIFI_SCANNING = BIT11;
static const int WIFIESP_EVENTSTATUSBIT_WIFI_SCAN_DONE = BIT12;
static const int WIFIESP_EVENTSTATUSBIT_WIFI_DNS_IDLE = BIT13;
static const int WIFIESP_EVENTSTATUSBIT_WIFI_DNS_DONE = BIT14;
