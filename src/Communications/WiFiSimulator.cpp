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

#if SIMULATOR

//-----------------------------------------------------------------------------
// Includes
//-----------------------------------------------------------------------------
#include <cstring>

#include "Communications/SindarinWiFi.hpp" // Class Header
#include "config.h"
#include "sindarin-debug.h"

//-----------------------------------------------------------------------------
// Object
//-----------------------------------------------------------------------------
// Create Object
CSindarinWiFi SindarinWiFi;

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
auto CSindarinWiFi::GetWiFiCredentials(wifi_config_t *conf) -> bool {
    strncpy((char *)conf->sta.ssid, "ssid", sizeof(conf->sta.ssid));
    strncpy((char *)conf->sta.password, "password", sizeof(conf->sta.password));
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
auto CSindarinWiFi::HasWiFiCredentials() -> bool { return true; }

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
auto CSindarinWiFi::ScanNetworks(bool asyncScan) -> int16_t { return true; }

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
auto CSindarinWiFi::ScanComplete() -> int16_t { return 1; }

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
auto CSindarinWiFi::GetSSID(uint8_t i) -> String { return "ssid"; }

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
auto CSindarinWiFi::GetRSSI(uint8_t i) -> int32_t { return 1; }

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
auto CSindarinWiFi::GetEncryptionType(uint8_t i) -> wifi_auth_mode_t { return WIFI_AUTH_WPA2_PSK; }

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
void CSindarinWiFi::ConnectWiFi(const std::string &ssid, const std::string &password) {
    log_i("Pretending to connect WiFi (power %d)", _wiFiPower);
    delay(1000);
    return;
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
auto CSindarinWiFi::ConnectWiFiIfCredentialsExist() -> bool {
    log_i("Pretending to connect WiFi (power %d)", _wiFiPower);
    delay(1000);
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
auto CSindarinWiFi::ConnectWiFiWithTimeout(int timeoutMs) -> bool {
    log_i("Pretending to connect WiFi (power %d)", _wiFiPower);
    delay(1000);
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
void CSindarinWiFi::DisconnectWiFi() { log_i("Pretending to disconnect WiFi"); }

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
auto CSindarinWiFi::Connected() -> bool { return true; }
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
auto CSindarinWiFi::GetLocalIP() -> IPAddress { return INADDR_NONE; }

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
wifiesp_status_t CSindarinWiFi::GetStatus() { return WIFIESP_STATUS_CONNECTED; }

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
auto CSindarinWiFi::GetMode() -> wifi_mode_t { return WIFI_MODE_STA; }

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
bool CSindarinWiFi::SetMode(wifi_mode_t mode) { return true; }

#endif