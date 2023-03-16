//______________________________________________________________________________
//------------------------------------------------------------------------------
/*!
    \file       SindarinWiFi.hpp

    \brief      WiFi wrapper for ESP32 Wireless driver Class Definition

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
#include <memory>
#include <string>

#include "Communications/WiFi/WiFiESP.hpp"

//-----------------------------------------------------------------------------
// Defines
//-----------------------------------------------------------------------------

//______________________________________________________________________________
//------------------------------------------------------------------------------
/*!
    \class      CSindarinWiFi

    \brief      WiFi wrapper for ESP32 Wireless driver Class
*/
//______________________________________________________________________________
//------------------------------------------------------------------------------
class CSindarinWiFi {

public:
    /// Default Constructor
    CSindarinWiFi();

    auto begin() -> esp_err_t;

    // Credentials
    auto getWiFiCredentials(wifi_config_t *conf) -> bool;
    auto hasWiFiCredentials() -> bool;

    // Scanning
    auto scanNetworks(
        const std::function<void(wifi_ap_record_t *apRecords, int recordCount)> &completeCB)
        -> esp_err_t;

    // Connection
    void connectWiFi(const std::string &ssid = "", const std::string &password = "");
    auto connectWiFiIfCredentialsExist() -> bool;
    auto connectWiFiWithTimeout(int timeoutMs = 20000) -> bool;
    void disconnectWiFi();
    auto connected() -> bool;

    // Settings
    void setWiFiPower(wifi_power_t power) { wiFiPower_ = power; }
    auto getWiFiPower() -> wifi_power_t { return wiFiPower_; }

    auto getStatus() -> wifiesp_status_t;

    auto getMode() -> wifi_mode_t;
    // auto setMode(wifi_mode_t mode) -> bool;

protected:
    //---------------------------------
    // Variables
    //---------------------------------
    bool began_ = false;
    wifi_power_t wiFiPower_ = WIFI_POWER_19_5dBm;
    bool autoReconnect_ = false;

    //---------------------------------
    // Functions
    //---------------------------------
};

// Global Object
extern CSindarinWiFi sindarinWiFi;
