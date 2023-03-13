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
#include <string>

#include "Communications/SindarinWiFi.hpp" // Class Header
#include "config.h"
#include "sindarin-debug.h"

//-----------------------------------------------------------------------------
// Object
//-----------------------------------------------------------------------------
// Create Object
CSindarinWiFi sindarinWiFi;

CSindarinWiFi::CSindarinWiFi() {
    // Base init variables
}

auto CSindarinWiFi::getWiFiCredentials(wifi_config_t *conf) -> bool {
    strncpy((char *)conf->sta.ssid, "ssid", sizeof(conf->sta.ssid));
    strncpy((char *)conf->sta.password, "password", sizeof(conf->sta.password));
    return true;
}

auto CSindarinWiFi::hasWiFiCredentials() -> bool { return true; }

auto CSindarinWiFi::scanNetworks(bool asyncScan) -> int16_t { return true; }

auto CSindarinWiFi::scanComplete() -> int16_t { return 1; }

auto CSindarinWiFi::getSsid(uint8_t i) -> std::string { return "ssid"; }

auto CSindarinWiFi::getRssi(uint8_t i) -> int32_t { return 1; }

auto CSindarinWiFi::getEncryptionType(uint8_t i) -> wifi_auth_mode_t { return WIFI_AUTH_WPA2_PSK; }

void CSindarinWiFi::connectWiFi(const std::string &ssid, const std::string &password) {
    log_i("Pretending to connect WiFi (power %d)", wiFiPower_);
    delay(1000);
    return;
}

auto CSindarinWiFi::connectWiFiIfCredentialsExist() -> bool {
    log_i("Pretending to connect WiFi (power %d)", wiFiPower_);
    delay(1000);
    return true;
}

auto CSindarinWiFi::connectWiFiWithTimeout(int timeoutMs) -> bool {
    log_i("Pretending to connect WiFi (power %d)", wiFiPower_);
    delay(1000);
    return true;
}

void CSindarinWiFi::disconnectWiFi() { log_i("Pretending to disconnect WiFi"); }

auto CSindarinWiFi::connected() -> bool { return true; }

auto CSindarinWiFi::getLocalIp() -> wifiesp_ip4address { return IPADDRESS_NONE; }

auto CSindarinWiFi::getLocalIpStr() -> std::string { return "00.00.00.00"; }

wifiesp_status_t CSindarinWiFi::getStatus() { return WIFIESP_STATUS_CONNECTED; }

auto CSindarinWiFi::getMode() -> wifi_mode_t { return WIFI_MODE_STA; }

bool CSindarinWiFi::setMode(wifi_mode_t mode) { return true; }

#endif