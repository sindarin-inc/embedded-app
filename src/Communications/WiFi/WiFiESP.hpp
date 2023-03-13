//______________________________________________________________________________
//------------------------------------------------------------------------------
/*!
    \file       WiFiESP.hpp

    \brief      WiFi ESP32 Wireless driver Class Definition

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
#include <string>

#include "WiFiESPDefs.hpp"
// #include <string.h>
#include "esp_event.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "lwip/err.h"
#include "lwip/sys.h"
#include "nvs_flash.h"

//------------------------------------------------------------------------------
// Defines
//------------------------------------------------------------------------------

//______________________________________________________________________________
//------------------------------------------------------------------------------
/*!
    \class      CWiFiESP

    \brief      WiFi wrapper for ESP32 Wireless driver Class
*/
//______________________________________________________________________________
//------------------------------------------------------------------------------
class CWiFiESP {

public:
    /// Default Constructor
    CWiFiESP();

    auto getStatus() -> wifiesp_status_t { return _status; }

    // Modes
    auto mode(wifi_mode_t) -> bool;
    auto getMode() -> wifi_mode_t;
    auto enableSTA(bool enable) -> bool;

    // Scan
    auto scanNetworks() -> int16_t;
    // Scan Information
    auto scanCount() -> int16_t { return _ap_count; }
    auto ssid(uint8_t networkItem) -> std::string;
    auto encryptionType(uint8_t networkItem) -> wifi_auth_mode_t;
    auto rssi(uint8_t networkItem) -> int32_t;

    // Power
    auto setTxPower(wifi_power_t power) -> bool;
    auto getTxPower() -> wifi_power_t;

    // Hostname
    auto getHostname() -> const char *;
    auto setHostname(const char *hostname) -> void;
    // auto hostname(const std::string &aHostname) -> bool { return setHostname(aHostname.c_str());
    // }

    // Start Wifi
    auto begin(const char *ssid, const char *passphrase = nullptr, int32_t channel = 0,
               const uint8_t *bssid = nullptr, bool connect = true) -> void;
    auto begin(char *ssid, char *passphrase = nullptr, int32_t channel = 0,
               const uint8_t *bssid = nullptr, bool connect = true) -> void;
    auto begin() -> void;

    // Auto Reconnect
    auto setAutoReconnect(bool autoReconnect) -> bool;
    auto getAutoReconnect() -> bool;

    // Connection
    auto reconnect() -> bool;
    auto disconnect(bool wifioff = false, bool eraseap = false) -> bool;
    auto isConnected() -> bool;
    auto localIP() -> wifiesp_ip4address;
    auto setConnected(bool connected) -> void {
        _status = (connected) ? WIFIESP_STATUS_CONNECTED : WIFIESP_STATUS_DISCONNECTED;
    }

protected:
    //---------------------------------
    // Variables
    //---------------------------------
    wifiesp_status_t _status = WIFIESP_STATUS_IDLE_STATUS;
    // Scan Access Point Info
    esp_netif_t *_sta_netif = nullptr;
    wifi_ap_record_t _ap_info[WIFIESP_DEFAULT_SCAN_LIST_SIZE];
    uint16_t _ap_count = 0;

    char _default_hostname[32] = {
        0,
    };
    char _default_password[64] = {
        0,
    };
    bool _autoReconnect = true;

    //---------------------------------
    // Functions
    //---------------------------------
};

//-----------------------------------------------------------------------------
// Extern Object
//-----------------------------------------------------------------------------
// Global Object
extern CWiFiESP wiFiEsp;
