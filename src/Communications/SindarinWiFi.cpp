//______________________________________________________________________________
//------------------------------------------------------------------------------
/*!
    \file       SindarinWiFi.cpp

    \brief      WiFi wrapper for ESP32 Wireless driver Class Implementation

    Copyright (c) 2023 Sindarin, Inc. - All Right Reserved

    The code contained in this file is confidential and may only be used
    under license and/or written consent of Sindarin.
*/
//______________________________________________________________________________
//------------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// Includes
//-----------------------------------------------------------------------------
#include "Communications/SindarinWiFi.hpp" // Class Header

#include <cstring>

#include "config.h"
#include "sindarin-debug.h"

//-----------------------------------------------------------------------------
// Object
//-----------------------------------------------------------------------------
// Create Object
CSindarinWiFi sindarinWiFi;

//______________________________________________________________________________
//------------------------------------------------------------------------------
/*!
    \fn     CSindarinWiFi

    \brief  Constructor
    \param  N/A
    \return N/A
*/
//______________________________________________________________________________
//------------------------------------------------------------------------------
CSindarinWiFi::CSindarinWiFi() {
    // Base init variables
}

//______________________________________________________________________________
//------------------------------------------------------------------------------
/*!
    \fn     GetWiFiCredentials

    \brief  Get the current Wifi credentials
    \param  pConf - Return value - Pointer to the config to return into
    \return bool - true = success
*/
//______________________________________________________________________________
//------------------------------------------------------------------------------
auto CSindarinWiFi::getWiFiCredentials(wifi_config_t *conf) -> bool {
    // Needs to be called before we can get the WiFi config
    if (!wiFiEsp.enableSTA(true)) {
        log_e("STA enable failed!");
        return false;
    }

    if (esp_wifi_get_config(WIFI_IF_STA, conf) != ESP_OK) {
        log_e("Failed to get WiFi config");
        return false;
    }
    if (strlen((const char *)conf->sta.ssid) == 0) {
        return false;
    }
    if (strlen((const char *)conf->sta.password) == 0) {
        log_w("Zero length WiFi password");
    }
    return true;
}

//______________________________________________________________________________
//------------------------------------------------------------------------------
/*!
    \fn     HasWiFiCredentials

    \brief  Do we have Wifi credentials
    \param  N/A
    \return bool - true = have credentials
*/
//______________________________________________________________________________
//------------------------------------------------------------------------------
auto CSindarinWiFi::hasWiFiCredentials() -> bool {
    wifi_config_t conf;
    return getWiFiCredentials(&conf);
}

//______________________________________________________________________________
//------------------------------------------------------------------------------
/*!
    \fn     ScanNetworks

    \brief  Scan for available wifi networks
    \param  asyncScan - perform a asynch scan
    \return int16_t - number networks found
*/
//______________________________________________________________________________
//------------------------------------------------------------------------------
auto CSindarinWiFi::scanNetworks(bool asyncScan) -> int16_t {
    return wiFiEsp.scanNetworks(asyncScan);
}

//______________________________________________________________________________
//------------------------------------------------------------------------------
/*!
    \fn     ScanComplete

    \brief  Called to get the scan in async mode
    \param  N/A
    \return int16_t - number networks found or negative = error
*/
//______________________________________________________________________________
//------------------------------------------------------------------------------
auto CSindarinWiFi::scanComplete() -> int16_t { return wiFiEsp.scanComplete(); }

//______________________________________________________________________________
//------------------------------------------------------------------------------
/*!
    \fn     GetSSID

    \brief  Get the SSID for a scanned network at provided offset
    \param  i - offset for scanned network
    \return String - the SSID
*/
//______________________________________________________________________________
//------------------------------------------------------------------------------
auto CSindarinWiFi::getSsid(uint8_t i) -> String { return wiFiEsp.ssid(i); }

//______________________________________________________________________________
//------------------------------------------------------------------------------
/*!
    \fn     GetRSSI

    \brief  Get the RSSI for a scanned network at provided offset
    \param  i - offset for scanned network
    \return int32_t - the RSSI
*/
//______________________________________________________________________________
//------------------------------------------------------------------------------
auto CSindarinWiFi::getRssi(uint8_t i) -> int32_t { return wiFiEsp.rssi(i); }

//______________________________________________________________________________
//------------------------------------------------------------------------------
/*!
    \fn     GetEncryptionType

    \brief  Get the Encryption for a scanned network at provided offset
    \param  i - offset for scanned network
    \return wifi_auth_mode_t - encryption type
*/
//______________________________________________________________________________
//------------------------------------------------------------------------------
auto CSindarinWiFi::getEncryptionType(uint8_t i) -> wifi_auth_mode_t {
    return wiFiEsp.encryptionType(i);
}

//______________________________________________________________________________
//------------------------------------------------------------------------------
/*!
    \fn     ConnectWiFi

    \brief  Connect via WiFi
    \param  ssid - SSID to connect to
    \param  password - Password for the network
    \return void
*/
//______________________________________________________________________________
//------------------------------------------------------------------------------
void CSindarinWiFi::connectWiFi(const std::string &ssid, const std::string &password) {
    // We have to have wifi sleep enabled otherwise Bluetooth can't work
    esp_wifi_set_ps(WIFI_PS_MIN_MODEM);

    // I'm not sure, but it might be that WiFi sleep before connecting to an AP could
    // be problematic.
    // if (!WiFiESP.setSleep(true)) {
    //     log_e("Failed to enable WiFi sleep");
    // }

    // If credentials are available, go ahead and attempt WiFi connection
    wifi_config_t conf;
    getWiFiCredentials(&conf);
    log_i("Found WiFi credentials for %s. Attempting connection", (const char *)conf.sta.ssid);
    wiFiEsp.setHostname(HOSTNAME);
    if (ssid.empty()) {
        wiFiEsp.begin();
    } else {
        wiFiEsp.begin(ssid.c_str(), password.c_str());
    }
    log_d("WiFi TX Power was %d", wiFiEsp.getTxPower());
    wiFiEsp.setTxPower(wiFiPower_);
    log_d("WiFi TX Power is now %d", wiFiEsp.getTxPower());
    wiFiEsp.setAutoReconnect(true);
}

//______________________________________________________________________________
//------------------------------------------------------------------------------
/*!
    \fn     ConnectWiFiIfCredentialsExist

    \brief  Connect via WiFi if we have credentials
    \param  N/A
    \return bool - true = connected
*/
//______________________________________________________________________________
//------------------------------------------------------------------------------
auto CSindarinWiFi::connectWiFiIfCredentialsExist() -> bool {
    if (!hasWiFiCredentials()) {
        log_d("No WiFi credentials found");
        return false;
    }
    connectWiFi();
    return true;
}

//______________________________________________________________________________
//------------------------------------------------------------------------------
/*!
    \fn     ConnectWiFiWithTimeout

    \brief  Connect via WiFi with a timeout to stop trying
    \param  timoutMS - timeout time in milliseconds
    \return void
*/
//______________________________________________________________________________
//------------------------------------------------------------------------------
auto CSindarinWiFi::connectWiFiWithTimeout(int timeoutMs) -> bool {
    auto startTime = millis();
    if (!connected()) {
        if (connectWiFiIfCredentialsExist()) {
            // Only wait if we had credentials
            // TODO: Would be nice to make this not a polling loop
            while (!connected() && (millis() - startTime) < timeoutMs) {
                delay(200);
            }
        }
        if (!connected()) {
            disconnectWiFi();
            return false;
        }
    }

    return true;
}

//______________________________________________________________________________
//------------------------------------------------------------------------------
/*!
    \fn     DisconnectWiFi

    \brief  Disconnect from WiFi
    \param  N/A
    \return void
*/
//______________________________________________________________________________
//------------------------------------------------------------------------------
void CSindarinWiFi::disconnectWiFi() {
    log_i("Disconnecting WiFi");
    wiFiEsp.setAutoReconnect(false);
    wiFiEsp.disconnect();
    wiFiEsp.mode(WIFI_MODE_NULL);
}

//______________________________________________________________________________
//------------------------------------------------------------------------------
/*!
    \fn     Connected

    \brief  Are we currently connected to network
    \param  N/A
    \return bool - true = connected
*/
//______________________________________________________________________________
//------------------------------------------------------------------------------
auto CSindarinWiFi::connected() -> bool { return wiFiEsp.isConnected(); }

//______________________________________________________________________________
//------------------------------------------------------------------------------
/*!
    \fn     GetLocalIP

    \brief  Get the station interface IP address
    \param  N/A
    \return IPAddress = 0 if not cennected
*/
//______________________________________________________________________________
//------------------------------------------------------------------------------
auto CSindarinWiFi::getLocalIp() -> IPAddress { return wiFiEsp.localIP(); }

//______________________________________________________________________________
//------------------------------------------------------------------------------
/*!
    \fn     GetStatus

    \brief  Return Connection status
    \param  N/A
    \return wifiesp_status_t
*/
//______________________________________________________________________________
//------------------------------------------------------------------------------
auto CSindarinWiFi::getStatus() -> wifiesp_status_t { return wiFiEsp.status(); }

//______________________________________________________________________________
//------------------------------------------------------------------------------
/*!
    \fn     GetMode

    \brief  Return Wifi mode
    \param  N/A
    \return wifi_mode_t
*/
//______________________________________________________________________________
//------------------------------------------------------------------------------
auto CSindarinWiFi::getMode() -> wifi_mode_t { return wiFiEsp.getMode(); }

//______________________________________________________________________________
//------------------------------------------------------------------------------
/*!
    \fn     SetMode

    \brief  Set a new mode
    \param  wifi_mode_t - mode to set
    \return bool - true= mode set
*/
//______________________________________________________________________________
//------------------------------------------------------------------------------
auto CSindarinWiFi::setMode(wifi_mode_t mode) -> bool { return wiFiEsp.mode(mode); }
