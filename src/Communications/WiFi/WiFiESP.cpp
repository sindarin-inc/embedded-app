//______________________________________________________________________________
//------------------------------------------------------------------------------
/*!
    \file       WiFiESP.cpp

    \brief      WiFi ESP32 Wireless driver Class Implementation

    Copyright (c) 2023 Sindarin, Inc. - All Right Reserved

    The code contained in this file is confidential and may only be used
    under license and/or written consent of Sindarin.
*/
//______________________________________________________________________________
//------------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// Includes
//-----------------------------------------------------------------------------
#include "WiFiESP.hpp" // Class Header

#include "WiFiESPTask.hpp" // Wifi Task
#include "config.h"
#include "sdkconfig.h"
#include "sindarin-debug.h"
// Std Libs
#include <cinttypes>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <vector>
// Base Wifi
#include "IPAddress.h"
#include "IPv6Address.h"
#include "dhcpserver/dhcpserver_options.h"
#include "esp_smartconfig.h"
#include "esp_wifi.h"
#include "wifi_provisioning/manager.h"
// ESP Libs
#include "esp32-hal.h"
#include "esp_err.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_system.h"
#include "esp_wpa2.h"
#include "nvs_flash.h"
// RTOS Lib
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/task.h"

//-----------------------------------------------------------------------------
// Object
//-----------------------------------------------------------------------------
// Create Object
CWiFiESP wiFiEsp;

//------------------------------------------------------------------------------
// Statics
//------------------------------------------------------------------------------
// STA Status
static wifiesp_status_t _sta_status = WIFIESP_STATUS_NO_WIFI;
// STA Status Group
static EventGroupHandle_t _sta_status_group = nullptr;

// Is Sleep Enabled
static wifi_ps_type_t _sleepEnabled = WIFI_PS_MIN_MODEM;

// Scan Asyn
static bool _scanAsync = false;
// Scan times
static uint32_t _scanStarted = 0;
static uint32_t _scanTimeout = 10000;
static uint16_t _scanCount = 0;
// Scan result
static void *_scanResult = nullptr;

// default hostname
static char _default_hostname[32] = {
    0,
};

// Create a callback type
typedef struct WifiespEventCbTList {
    static wifi_event_id_t currentId;
    wifi_event_id_t id;
    wifiesp_event_cb_t cb;
    wifiesp_event_func_cb_t fcb;
    wifiesp_event_cb_sys_t scb;
    wifiesp_event_id_t event{WIFIESP_EVENT_WIFI_READY};
    WifiespEventCbTList() : id(currentId++), cb(nullptr), fcb(nullptr), scb(nullptr) {}
} wifiesp_event_cb_tList_t;

// Callback ID - init to 1
wifi_event_id_t WifiespEventCbTList::currentId = 1;

// Create a static std::vectors of for the callback event list
static std::vector<wifiesp_event_cb_tList_t> _eventCBList;

// Net interfaces
static esp_netif_t *_netIfs[ESP_IF_MAX] = {nullptr, nullptr, nullptr};

//______________________________________________________________________________
//------------------------------------------------------------------------------
/*!
    \fn     CWiFiESP

    \brief  Constructor
    \param  N/A
    \return N/A
*/
//______________________________________________________________________________
//------------------------------------------------------------------------------
CWiFiESP::CWiFiESP() {
    // Base init variables
}

//______________________________________________________________________________
//------------------------------------------------------------------------------
/*!
    \fn     mode

    \brief  Set Wifi Mode
    \param  m WiFiMode_t
    \return bool - true=success
*/
//______________________________________________________________________________
//------------------------------------------------------------------------------
auto CWiFiESP::mode(wifi_mode_t m) -> bool {
    wifi_mode_t cm = getMode();
    if (cm == m) {
        return true;
    }
    if (!cm && m) {
        if (!wifiLowLevelInit(persistent_)) {
            return false;
        }
    } else if (cm && !m) {
        return espWiFiStop();
    }

    esp_err_t err;
    if (m & WIFI_MODE_STA) {
        err = setEspInterfaceHostname(ESP_IF_WIFI_STA, getEspNetifHostname());
        if (err) {
            log_e("Could not set hostname! %d", err);
            return false;
        }
    }
    err = esp_wifi_set_mode(m);
    if (err) {
        log_e("Could not set mode! %d", err);
        return false;
    }
    if (longRange_) {
        if (m & WIFI_MODE_STA) {
            err = esp_wifi_set_protocol(WIFI_IF_STA, WIFI_PROTOCOL_LR);
            if (err != ESP_OK) {
                log_e("Could not enable long range on STA! %d", err);
                return false;
            }
        }
        if (m & WIFI_MODE_AP) {
            err = esp_wifi_set_protocol(WIFI_IF_AP, WIFI_PROTOCOL_LR);
            if (err != ESP_OK) {
                log_e("Could not enable long range on AP! %d", err);
                return false;
            }
        }
    }
    if (!espWiFiStart()) {
        return false;
    }

    return true;
}

//______________________________________________________________________________
//------------------------------------------------------------------------------
/*!
    \fn     getMode

    \brief  Get Wifi mode
    \param  N/A
    \return wifi_mode_t - current mode settings
*/
//______________________________________________________________________________
//------------------------------------------------------------------------------
auto CWiFiESP::getMode() -> wifi_mode_t {
    if (!lowLevelInitDone_ || !espWifiStarted_) {
        return WIFI_MODE_NULL;
    }
    wifi_mode_t mode;
    if (esp_wifi_get_mode(&mode) != ESP_OK) {
        log_w("WiFi not started");
        return WIFI_MODE_NULL;
    }
    return mode;
}

//______________________________________________________________________________
//------------------------------------------------------------------------------
/*!
    \fn     enableSTA

    \brief  control STA mode
    \param  enable bool
    \return bool true=ok
*/
//______________________________________________________________________________
//------------------------------------------------------------------------------
auto CWiFiESP::enableSTA(bool enable) -> bool {
    wifi_mode_t currentMode = getMode();
    bool isEnabled = ((currentMode & WIFI_MODE_STA) != 0);

    if (isEnabled != enable) {
        if (enable) {
            return mode((wifi_mode_t)(currentMode | WIFI_MODE_STA));
        }
        return mode((wifi_mode_t)(currentMode & (~WIFI_MODE_STA)));
    }
    return true;
}

//______________________________________________________________________________
//------------------------------------------------------------------------------
/*!
    \fn     scanNetworks

    \brief  Start scan WiFi networks available
    \param  async run in async mode
        \param  show_hidden show hidden networks
    \return int16_t - Number of discovered networks
*/
//______________________________________________________________________________
//------------------------------------------------------------------------------
auto CWiFiESP::scanNetworks(bool async, bool showHidden, bool passive, uint32_t maxMsPerChan,
                            uint8_t channel) -> int16_t {
    if (getEventStatusBits() & WIFIESP_EVENTSTATUSBIT_WIFI_SCANNING) {
        return WIFI_SCAN_RUNNING;
    }

    _scanTimeout = maxMsPerChan * 20;
    _scanAsync = async;

    enableSTA(true);

    scanDelete();

    wifi_scan_config_t config;
    config.ssid = nullptr;
    config.bssid = nullptr;
    config.channel = channel;
    config.show_hidden = showHidden;
    if (passive) {
        config.scan_type = WIFI_SCAN_TYPE_PASSIVE;
        config.scan_time.passive = maxMsPerChan;
    } else {
        config.scan_type = WIFI_SCAN_TYPE_ACTIVE;
        config.scan_time.active.min = 100;
        config.scan_time.active.max = maxMsPerChan;
    }
    if (esp_wifi_scan_start(&config, false) == ESP_OK) {
        _scanStarted = millis();
        if (!_scanStarted) { // Prevent 0 from millis overflow
            ++_scanStarted;
        }

        clearEventStatusBits(WIFIESP_EVENTSTATUSBIT_WIFI_SCAN_DONE);
        setEventStatusBits(WIFIESP_EVENTSTATUSBIT_WIFI_SCANNING);

        if (_scanAsync) {
            return WIFI_SCAN_RUNNING;
        }
        if (waitEventStatusBits(WIFIESP_EVENTSTATUSBIT_WIFI_SCAN_DONE, 10000)) {
            return (int16_t)_scanCount;
        }
    }
    return WIFI_SCAN_FAILED;
}

//______________________________________________________________________________
//------------------------------------------------------------------------------
/*!
    \fn     _scanDoneCB

    \brief  Scan completed callback
    \param  N/A
    \return void
*/
//______________________________________________________________________________
//------------------------------------------------------------------------------
void CWiFiESP::scanDoneCb() {
    esp_wifi_scan_get_ap_num(&(_scanCount));
    if (_scanCount) {
        _scanResult = new wifi_ap_record_t[_scanCount];
        if (!_scanResult || esp_wifi_scan_get_ap_records(
                                &(_scanCount), (wifi_ap_record_t *)_scanResult) != ESP_OK) {
            _scanCount = 0;
        }
    }
    _scanStarted = 0; // Reset after a scan is completed for normal behavior
    setEventStatusBits(WIFIESP_EVENTSTATUSBIT_WIFI_SCAN_DONE);
    clearEventStatusBits(WIFIESP_EVENTSTATUSBIT_WIFI_SCANNING);
}

//______________________________________________________________________________
//------------------------------------------------------------------------------
/*!
    \fn     scanComplete

    \brief  called to get the scan state in Async mode
    \param  N/A
    \return int16_t - -1 if scan not fin, -2 if scan not triggered
*/
//______________________________________________________________________________
//------------------------------------------------------------------------------
auto CWiFiESP::scanComplete() -> int16_t {
    if (_scanStarted &&
        (millis() - _scanStarted) > _scanTimeout) { // Check is scan was started and if the delay
                                                    // expired, return WIFI_SCAN_FAILED in this case
        clearEventStatusBits(WIFIESP_EVENTSTATUSBIT_WIFI_SCANNING);
        return WIFI_SCAN_FAILED;
    }

    if (getEventStatusBits() & WIFIESP_EVENTSTATUSBIT_WIFI_SCAN_DONE) {
        return _scanCount;
    }

    if (getEventStatusBits() & WIFIESP_EVENTSTATUSBIT_WIFI_SCANNING) {
        return WIFI_SCAN_RUNNING;
    }

    return WIFI_SCAN_FAILED;
}

//______________________________________________________________________________
//------------------------------------------------------------------------------
/*!
    \fn     scanDelete

    \brief  delete last scan result from RAM
    \param  N/A
    \return void
*/
//______________________________________________________________________________
//------------------------------------------------------------------------------
void CWiFiESP::scanDelete() {
    clearEventStatusBits(WIFIESP_EVENTSTATUSBIT_WIFI_SCAN_DONE);
    if (_scanResult) {
        delete[] reinterpret_cast<wifi_ap_record_t *>(_scanResult);
        _scanResult = nullptr;
        _scanCount = 0;
    }
}

//______________________________________________________________________________
//------------------------------------------------------------------------------
/*!
    \fn     getScanInfoByIndex

    \brief  Get scan info
    \param  i specify from which network item want to get the information
    \return bss_info * - info
*/
//______________________________________________________________________________
//------------------------------------------------------------------------------
auto CWiFiESP::getScanInfoByIndex(int i) -> void * {
    if (!_scanResult || (size_t)i >= _scanCount) {
        return nullptr;
    }
    return reinterpret_cast<wifi_ap_record_t *>(_scanResult) + i;
}

//______________________________________________________________________________
//------------------------------------------------------------------------------
/*!
    \fn     getNetworkInfo

    \brief  loads all infos from a scanned wifi in to the ptr parameters
        \param  networkItem uint8_t
        \param  ssid  const char**
        \param  encryptionType uint8_t *
        \param  RSSI int32_t *
        \param  BSSID uint8_t **
        \param  channel int32_t *
    \return bool - true=ok
*/
//______________________________________________________________________________
//------------------------------------------------------------------------------
auto CWiFiESP::getNetworkInfo(uint8_t i, String &ssid, uint8_t &encType, int32_t &rssi,
                              uint8_t *&bssid, int32_t &channel) -> bool {
    auto *it = reinterpret_cast<wifi_ap_record_t *>(getScanInfoByIndex(i));
    if (!it) {
        return false;
    }
    ssid = (const char *)it->ssid;
    encType = it->authmode;
    rssi = it->rssi;
    bssid = it->bssid;
    channel = it->primary;
    return true;
}

//______________________________________________________________________________
//------------------------------------------------------------------------------
/*!
    \fn     SSID

    \brief  Return the SSID discovered during the network scan.
    \param  i specify from which network item want to get the information
    \return String - ssid string of the specified item on the networks scanned list
*/
//______________________________________________________________________________
//------------------------------------------------------------------------------
auto CWiFiESP::ssid(uint8_t i) -> String {
    auto *it = reinterpret_cast<wifi_ap_record_t *>(getScanInfoByIndex(i));
    if (!it) {
        return {};
    }
    return {reinterpret_cast<const char *>(it->ssid)};
}

//______________________________________________________________________________
//------------------------------------------------------------------------------
/*!
    \fn     encryptionType

    \brief  Return the encryption type of the networks discovered during the scanNetworks
    \param  i specify from which network item want to get the information
    \return encryption type (enum wl_enc_type) of the specified item on the networks scanned list
*/
//______________________________________________________________________________
//------------------------------------------------------------------------------
auto CWiFiESP::encryptionType(uint8_t i) -> wifi_auth_mode_t {
    auto *it = reinterpret_cast<wifi_ap_record_t *>(getScanInfoByIndex(i));
    if (!it) {
        return WIFI_AUTH_OPEN;
    }
    return it->authmode;
}

//______________________________________________________________________________
//------------------------------------------------------------------------------
/*!
    \fn     RSSI

    \brief  Return the RSSI of the networks discovered during the scanNetworks
    \param  i specify from which network item want to get the information
    \return int32_t - value of RSSI of the specified item on the networks scanned list
*/
//______________________________________________________________________________
//------------------------------------------------------------------------------
auto CWiFiESP::rssi(uint8_t i) -> int32_t {
    auto *it = reinterpret_cast<wifi_ap_record_t *>(getScanInfoByIndex(i));
    if (!it) {
        return 0;
    }
    return it->rssi;
}

//______________________________________________________________________________
//------------------------------------------------------------------------------
/*!
    \fn     BSSID

    \brief  return MAC / BSSID of scanned wifi
    \param  i specify from which network item want to get the information
    \return uint8_t * MAC / BSSID of scanned wifi
*/
//______________________________________________________________________________
//------------------------------------------------------------------------------
auto CWiFiESP::bssid(uint8_t i) -> uint8_t * {
    auto *it = reinterpret_cast<wifi_ap_record_t *>(getScanInfoByIndex(i));
    if (!it) {
        return nullptr;
    }
    return it->bssid;
}

//______________________________________________________________________________
//------------------------------------------------------------------------------
/*!
    \fn     BSSIDstr

    \brief  return MAC / BSSID of scanned wifi
    \param  i specify from which network item want to get the information
    \return String MAC / BSSID of scanned wifi
*/
//______________________________________________________________________________
//------------------------------------------------------------------------------
auto CWiFiESP::bssiDstr(uint8_t i) -> String {
    char mac[18] = {0};
    auto *it = reinterpret_cast<wifi_ap_record_t *>(getScanInfoByIndex(i));
    if (!it) {
        return {};
    }
    sprintf(mac, "%02X:%02X:%02X:%02X:%02X:%02X", it->bssid[0], it->bssid[1], it->bssid[2],
            it->bssid[3], it->bssid[4], it->bssid[5]);
    return {mac};
}

//______________________________________________________________________________
//------------------------------------------------------------------------------
/*!
    \fn     scan_channel

    \brief  get the channel
    \param  i specify from which network item want to get the information
    \return int32_t - channel
*/
//______________________________________________________________________________
//------------------------------------------------------------------------------
auto CWiFiESP::scanChannel(uint8_t i) -> int32_t {
    auto *it = reinterpret_cast<wifi_ap_record_t *>(getScanInfoByIndex(i));
    if (!it) {
        return 0;
    }
    return it->primary;
}

//______________________________________________________________________________
//------------------------------------------------------------------------------
/*!
    \fn     setSleep

    \brief  control modem sleep when only in STA mode
    \param  enabled - enable sleep
    \return bool - true=ok
*/
//______________________________________________________________________________
//------------------------------------------------------------------------------
auto CWiFiESP::setSleep(bool enabled) -> bool {
    return setSleep(enabled ? WIFI_PS_MIN_MODEM : WIFI_PS_NONE);
}

//______________________________________________________________________________
//------------------------------------------------------------------------------
/*!
    \fn     setSleep

    \brief  set Sleep
    \param  mode wifi_ps_type_t
    \return bool - true=ok
*/
//______________________________________________________________________________
//------------------------------------------------------------------------------
auto CWiFiESP::setSleep(wifi_ps_type_t sleepType) -> bool {
    if (sleepType != _sleepEnabled) {
        _sleepEnabled = sleepType;
        if ((getMode() & WIFI_MODE_STA) != 0) {
            if (esp_wifi_set_ps(_sleepEnabled) != ESP_OK) {
                log_e("esp_wifi_set_ps failed!");
                return false;
            }
        }
        return true;
    }
    return false;
}

//______________________________________________________________________________
//------------------------------------------------------------------------------
/*!
    \fn     getSleep

    \brief  get modem sleep enabled
    \param  N/A
    \return bool - true if modem sleep is enabled
*/
//______________________________________________________________________________
//------------------------------------------------------------------------------
auto CWiFiESP::getSleep() -> wifi_ps_type_t { return _sleepEnabled; }

//______________________________________________________________________________
//------------------------------------------------------------------------------
/*!
    \fn     setTxPower

    \brief  control wifi tx power
    \param  power enum maximum wifi tx power
    \return bool - true=ok
*/
//______________________________________________________________________________
//------------------------------------------------------------------------------
auto CWiFiESP::setTxPower(wifi_power_t power) -> bool {
    if ((getEventStatusBits() &
         (WIFIESP_EVENTSTATUSBIT_STA_STARTED | WIFIESP_EVENTSTATUSBIT_AP_STARTED)) == 0) {
        log_w("Neither AP or STA has been started");
        return false;
    }
    return esp_wifi_set_max_tx_power(power) == ESP_OK;
}

//______________________________________________________________________________
//------------------------------------------------------------------------------
/*!
    \fn     getTxPower

    \brief  Get Tx Powere
    \param  N/A
    \return power enum maximum wifi tx power
*/
//______________________________________________________________________________
//------------------------------------------------------------------------------
auto CWiFiESP::getTxPower() -> wifi_power_t {
    int8_t power;
    if ((getEventStatusBits() &
         (WIFIESP_EVENTSTATUSBIT_STA_STARTED | WIFIESP_EVENTSTATUSBIT_AP_STARTED)) == 0) {
        log_w("Neither AP or STA has been started");
        return WIFI_POWER_19_5dBm;
    }
    if (esp_wifi_get_max_tx_power(&power)) {
        return WIFI_POWER_19_5dBm;
    }
    return (wifi_power_t)power;
}

//______________________________________________________________________________
//------------------------------------------------------------------------------
/*!
    \fn     getHostname

    \brief  Get Hostname
    \param  N/A
    \return char* - hostname
*/
//______________________________________________________________________________
//------------------------------------------------------------------------------
auto CWiFiESP::getHostname() -> const char * { return getEspNetifHostname(); }

//______________________________________________________________________________
//------------------------------------------------------------------------------
/*!
    \fn     SetHostname

    \brief  Set Hostname
    \param  char* - hostname
    \return bool - true=ok
*/
//______________________________________________________________________________
//------------------------------------------------------------------------------

auto CWiFiESP::setHostname(const char *hostname) -> bool {
    setEspNetifHostname(hostname);
    return true;
}

//______________________________________________________________________________
//------------------------------------------------------------------------------
/*!
    \fn     begin

    \brief  Start Wifi connection with a WPA2 Enterprise AP
            if passphrase is set the most secure supported mode will be automatically selected
    \param ssid const char*          Pointer to the SSID string.
    \param method wpa2_method_t      The authentication method of WPA2 (WIFIESP_WPA2_AUTH_TLS,
   WIFIESP_WPA2_AUTH_PEAP, WIFIESP_WPA2_AUTH_TTLS) \param wpa2_identity  const char* Pointer to the
   entity \param wpa2_username  const char*          Pointer to the username \param password const
   char *     Pointer to the password. \param ca_pem const char*        Pointer to a string with the
   contents of a  .pem  file with CA cert \param client_crt const char*        Pointer to a string
   with the contents of a .crt file with client cert \param client_key const char*        Pointer to
   a string with the contants of a .key file with client key \param bssid uint8_t[6] Optional. BSSID
   / MAC of AP \param channel                   Optional. Channel of AP \param connect Optional.
   call connect \return wifiesp_status_t - status
*/
//______________________________________________________________________________
//------------------------------------------------------------------------------
auto CWiFiESP::begin(const char *wpa2Ssid, wifiesp_wpa2_auth_method_t method,
                     const char *wpa2Identity, const char *wpa2Username, const char *wpa2Password,
                     const char *caPem, const char *clientCrt, const char *clientKey,
                     int32_t channel, const uint8_t *bssid, bool connect) -> wifiesp_status_t {
    if (!enableSTA(true)) {
        log_e("STA enable failed!");
        return WIFIESP_STATUS_CONNECT_FAILED;
    }

    if (!wpa2Ssid || *wpa2Ssid == 0x00 || strlen(wpa2Ssid) > 32) {
        log_e("SSID too long or missing!");
        return WIFIESP_STATUS_CONNECT_FAILED;
    }

    if (wpa2Identity && strlen(wpa2Identity) > 64) {
        log_e("identity too long!");
        return WIFIESP_STATUS_CONNECT_FAILED;
    }

    if (wpa2Username && strlen(wpa2Username) > 64) {
        log_e("username too long!");
        return WIFIESP_STATUS_CONNECT_FAILED;
    }

    if (wpa2Password && strlen(wpa2Password) > 64) {
        log_e("password too long!");
    }

    if (caPem) {
        esp_wifi_sta_wpa2_ent_set_ca_cert((uint8_t *)caPem, strlen(caPem));
    }

    if (clientCrt) {
        esp_wifi_sta_wpa2_ent_set_cert_key((uint8_t *)clientCrt, strlen(clientCrt),
                                           (uint8_t *)clientKey, strlen(clientKey), nullptr, 0);
    }

    esp_wifi_sta_wpa2_ent_set_identity((uint8_t *)wpa2Identity, strlen(wpa2Identity));
    if (method == WIFIESP_WPA2_AUTH_PEAP || method == WIFIESP_WPA2_AUTH_TTLS) {
        esp_wifi_sta_wpa2_ent_set_username((uint8_t *)wpa2Username, strlen(wpa2Username));
        esp_wifi_sta_wpa2_ent_set_password((uint8_t *)wpa2Password, strlen(wpa2Password));
    }
    esp_wifi_sta_wpa2_ent_enable(); // set config settings to enable function
    begin(wpa2Ssid);                // connect to wifi

    return status();
}

//______________________________________________________________________________
//------------------------------------------------------------------------------
/*!
    \fn     begin

    \brief  Start Wifi connection with a WPA2 Enterprise AP
            if passphrase is set the most secure supported mode will be automatically selected
    \param ssid const char*          Pointer to the SSID string.
    \param passphrase const char *   Optional. Passphrase. Valid characters in a passphrase must be
   between ASCII 32-126 (decimal). \param bssid uint8_t[6]          Optional. BSSID / MAC of AP
    \param channel                   Optional. Channel of AP
    \param connect                   Optional. call connect
    \return wifiesp_status_t - status
*/
//______________________________________________________________________________
//------------------------------------------------------------------------------
auto CWiFiESP::begin(const char *ssid, const char *passphrase, int32_t channel,
                     const uint8_t *bssid, bool connect) -> wifiesp_status_t {

    if (!enableSTA(true)) {
        log_e("STA enable failed!");
        return WIFIESP_STATUS_CONNECT_FAILED;
    }

    if (!ssid || *ssid == 0x00 || strlen(ssid) > 32) {
        log_e("SSID too long or missing!");
        return WIFIESP_STATUS_CONNECT_FAILED;
    }

    if (passphrase && strlen(passphrase) > 64) {
        log_e("passphrase too long!");
        return WIFIESP_STATUS_CONNECT_FAILED;
    }

    wifi_config_t conf;
    memset(&conf, 0, sizeof(wifi_config_t));
    wifiStrncpy(reinterpret_cast<char *>(conf.sta.ssid), ssid, 32);

    if (passphrase) {
        wifiStrncpy(reinterpret_cast<char *>(conf.sta.password), passphrase, 64);
    }

    if (channel == 0) {
        // If no specific channel specified, then do an slower WIFI_ALL_CHANNEL_SCAN
        wifiStaConfig(&conf, ssid, passphrase, bssid, channel, WIFI_ALL_CHANNEL_SCAN);
    } else {
        wifiStaConfig(&conf, ssid, passphrase, bssid, channel, WIFI_FAST_SCAN);
    }

    wifi_config_t currentConf;
    if (esp_wifi_get_config((wifi_interface_t)ESP_IF_WIFI_STA, &currentConf) != ESP_OK) {
        log_e("get current config failed!");
        return WIFIESP_STATUS_CONNECT_FAILED;
    }
    if (!staConfigEqual(currentConf, conf)) {
        if (esp_wifi_disconnect()) {
            log_e("disconnect failed!");
            return WIFIESP_STATUS_CONNECT_FAILED;
        }

        if (esp_wifi_set_config((wifi_interface_t)ESP_IF_WIFI_STA, &conf) != ESP_OK) {
            log_e("set config failed!");
            return WIFIESP_STATUS_CONNECT_FAILED;
        }
    } else if (status() == WIFIESP_STATUS_CONNECTED) {
        return WIFIESP_STATUS_CONNECTED;
    } else {
        if (esp_wifi_set_config((wifi_interface_t)ESP_IF_WIFI_STA, &conf) != ESP_OK) {
            log_e("set config failed!");
            return WIFIESP_STATUS_CONNECT_FAILED;
        }
    }

    // if(!_useStaticIp){
    if (setEspInterfaceIp(ESP_IF_WIFI_STA) != ESP_OK) {
        return WIFIESP_STATUS_CONNECT_FAILED;
    }
    //}

    if (connect) {
        if (esp_wifi_connect() != ESP_OK) {
            log_e("connect failed!");
            return WIFIESP_STATUS_CONNECT_FAILED;
        }
    }

    return status();
}

//______________________________________________________________________________
//------------------------------------------------------------------------------
/*!
    \fn     begin

    \brief  Start Wifi connection with a WPA2 Enterprise AP
            if passphrase is set the most secure supported mode will be automatically selected
    \param ssid char*                Pointer to the SSID string.
    \param passphrase char *         Optional. Passphrase. Valid characters in a passphrase must be
   between ASCII 32-126 (decimal). \param bssid uint8_t[6]          Optional. BSSID / MAC of AP
    \param channel                   Optional. Channel of AP
    \param connect                   Optional. call connect
    \return wifiesp_status_t - status
*/
//______________________________________________________________________________
//------------------------------------------------------------------------------
auto CWiFiESP::begin(char *ssid, char *passphrase, int32_t channel, const uint8_t *bssid,
                     bool connect) -> wifiesp_status_t {
    return begin((const char *)ssid, (const char *)passphrase, channel, bssid, connect);
}

//______________________________________________________________________________
//------------------------------------------------------------------------------
/*!
    \fn     begin

    \brief  Use to connect to SDK config.
    \param N/A
    \return wifiesp_status_t - status
*/
//______________________________________________________________________________
//------------------------------------------------------------------------------
auto CWiFiESP::begin() -> wifiesp_status_t {

    if (!enableSTA(true)) {
        log_e("STA enable failed!");
        return WIFIESP_STATUS_CONNECT_FAILED;
    }

    wifi_config_t currentConf;
    if (esp_wifi_get_config((wifi_interface_t)ESP_IF_WIFI_STA, &currentConf) != ESP_OK ||
        esp_wifi_set_config((wifi_interface_t)ESP_IF_WIFI_STA, &currentConf) != ESP_OK) {
        log_e("config failed");
        return WIFIESP_STATUS_CONNECT_FAILED;
    }

    // if(!_useStaticIp && set_esp_interface_ip(ESP_IF_WIFI_STA) != ESP_OK) {
    if (setEspInterfaceIp(ESP_IF_WIFI_STA) != ESP_OK) {
        log_e("set ip failed!");
        return WIFIESP_STATUS_CONNECT_FAILED;
    }

    if (status() != WIFIESP_STATUS_CONNECTED) {
        esp_err_t err = esp_wifi_connect();
        if (err) {
            log_e("connect failed! 0x%x", err);
            return WIFIESP_STATUS_CONNECT_FAILED;
        }
    }

    return status();
}

//______________________________________________________________________________
//------------------------------------------------------------------------------
/*!
        \fn     setAutoReconnect

        \brief  Set Autoreconnect
        \param  autoReconnect - setting
        \return bool - true=ok
*/
//______________________________________________________________________________
//------------------------------------------------------------------------------
auto CWiFiESP::setAutoReconnect(bool autoReconnect) -> bool {
    autoReconnect_ = autoReconnect;
    return true;
}

//______________________________________________________________________________
//------------------------------------------------------------------------------
/*!
        \fn     getAutoReconnect

        \brief  Get Autoreconnect
        \param  N/A
        \return bool - autoReconnect setting
*/
//______________________________________________________________________________
//------------------------------------------------------------------------------
auto CWiFiESP::getAutoReconnect() -> bool { return autoReconnect_; }

//______________________________________________________________________________
//------------------------------------------------------------------------------
/*!
    \fn     reconnect

    \brief  will force a disconnect and then start reconnecting to AP
    \param  N/A
    \return bool - true=ok
*/
//______________________________________________________________________________
//------------------------------------------------------------------------------
auto CWiFiESP::reconnect() -> bool {
    if (getMode() & WIFI_MODE_STA) {
        if (esp_wifi_disconnect() == ESP_OK) {
            return esp_wifi_connect() == ESP_OK;
        }
    }
    return false;
}

//______________________________________________________________________________
//------------------------------------------------------------------------------
/*!
    \fn     disconnect

    \brief  Disconnect from the network
    \param  wifioff - turn off
    \return bool - true=ok
*/
//______________________________________________________________________________
//------------------------------------------------------------------------------
auto CWiFiESP::disconnect(bool wifioff, bool eraseap) -> bool {
    wifi_config_t conf;
    wifiStaConfig(&conf);

    if (getMode() & WIFI_MODE_STA) {
        if (eraseap) {
            if (esp_wifi_set_config((wifi_interface_t)ESP_IF_WIFI_STA, &conf)) {
                log_e("clear config failed!");
            }
        }
        if (esp_wifi_disconnect()) {
            log_e("disconnect failed!");
            return false;
        }
        if (wifioff) {
            return enableSTA(false);
        }
        return true;
    }

    return false;
}

//______________________________________________________________________________
//------------------------------------------------------------------------------
/*!
    \fn     isConnected

    \brief  is STA interface connected
    \param  N/A
    \return bool - true if STA is connected to an AP
*/
//______________________________________________________________________________
//------------------------------------------------------------------------------
auto CWiFiESP::isConnected() -> bool { return (status() == WIFIESP_STATUS_CONNECTED); }

//______________________________________________________________________________
//------------------------------------------------------------------------------
/*!
    \fn     localIP

    \brief  Get the station interface IP address.
    \param  N/A
    \return IPAddress station IP
*/
//______________________________________________________________________________
//------------------------------------------------------------------------------
auto CWiFiESP::localIP() -> IPAddress {
    if (getMode() == WIFI_MODE_NULL) {
        return {};
    }
    esp_netif_ip_info_t ip;
    if (esp_netif_get_ip_info(getEspInterfaceNetif(ESP_IF_WIFI_STA), &ip) != ESP_OK) {
        log_e("Netif Get IP Failed!");
        return {};
    }
    return {ip.ip.addr};
}

//______________________________________________________________________________
//------------------------------------------------------------------------------
/*!
    \fn     _setStatus

    \brief  Set STA Status
    \param  status - status to set
    \return void
*/
//______________________________________________________________________________
//------------------------------------------------------------------------------
void CWiFiESP::setStatus(wifiesp_status_t status) {
    if (!_sta_status_group) {
        _sta_status_group = xEventGroupCreate();
        if (!_sta_status_group) {
            log_e("STA Status Group Create Failed!");
            _sta_status = status;
            return;
        }
    }
    xEventGroupClearBits(_sta_status_group, 0x00FFFFFF);
    xEventGroupSetBits(_sta_status_group, status);
}

//______________________________________________________________________________
//------------------------------------------------------------------------------
/*!
    \fn     status

    \brief  Return STA Connection status
    \param  N/A
    \return one of the value defined in wifiesp_status_t
*/
//______________________________________________________________________________
//------------------------------------------------------------------------------
auto CWiFiESP::status() -> wifiesp_status_t {
    if (!_sta_status_group) {
        return _sta_status;
    }
    return (wifiesp_status_t)xEventGroupClearBits(_sta_status_group, 0);
}

//______________________________________________________________________________
//------------------------------------------------------------------------------
/*!
    \fn     _eventCallback

    \brief  callback for WiFi events
    \param  event - event for the vcallback
    \return esp_err_t - error
*/
//______________________________________________________________________________
//------------------------------------------------------------------------------
// Debug output logging
#if ARDUHAL_LOG_LEVEL >= ARDUHAL_LOG_LEVEL_DEBUG
const char *wifiespEventNames[] = {"WIFI_READY",
                                   "SCAN_DONE",
                                   "STA_START",
                                   "STA_STOP",
                                   "STA_CONNECTED",
                                   "STA_DISCONNECTED",
                                   "STA_AUTHMODE_CHANGE",
                                   "STA_GOT_IP",
                                   "STA_GOT_IP6",
                                   "STA_LOST_IP",
                                   "AP_START",
                                   "AP_STOP",
                                   "AP_STACONNECTED",
                                   "AP_STADISCONNECTED",
                                   "AP_STAIPASSIGNED",
                                   "AP_PROBEREQRECVED",
                                   "AP_GOT_IP6",
                                   "FTM_REPORT",
                                   "ETH_START",
                                   "ETH_STOP",
                                   "ETH_CONNECTED",
                                   "ETH_DISCONNECTED",
                                   "ETH_GOT_IP",
                                   "ETH_GOT_IP6",
                                   "WPS_ER_SUCCESS",
                                   "WPS_ER_FAILED",
                                   "WPS_ER_TIMEOUT",
                                   "WPS_ER_PIN",
                                   "WPS_ER_PBC_OVERLAP",
                                   "SC_SCAN_DONE",
                                   "SC_FOUND_CHANNEL",
                                   "SC_GOT_SSID_PSWD",
                                   "SC_SEND_ACK_DONE",
                                   "PROV_INIT",
                                   "PROV_DEINIT",
                                   "PROV_START",
                                   "PROV_END",
                                   "PROV_CRED_RECV",
                                   "PROV_CRED_FAIL",
                                   "PROV_CRED_SUCCESS"};
#endif
#if ARDUHAL_LOG_LEVEL >= ARDUHAL_LOG_LEVEL_WARN
const char *system_event_reasons[] = {"UNSPECIFIED",
                                      "AUTH_EXPIRE",
                                      "AUTH_LEAVE",
                                      "ASSOC_EXPIRE",
                                      "ASSOC_TOOMANY",
                                      "NOT_AUTHED",
                                      "NOT_ASSOCED",
                                      "ASSOC_LEAVE",
                                      "ASSOC_NOT_AUTHED",
                                      "DISASSOC_PWRCAP_BAD",
                                      "DISASSOC_SUPCHAN_BAD",
                                      "UNSPECIFIED",
                                      "IE_INVALID",
                                      "MIC_FAILURE",
                                      "4WAY_HANDSHAKE_TIMEOUT",
                                      "GROUP_KEY_UPDATE_TIMEOUT",
                                      "IE_IN_4WAY_DIFFERS",
                                      "GROUP_CIPHER_INVALID",
                                      "PAIRWISE_CIPHER_INVALID",
                                      "AKMP_INVALID",
                                      "UNSUPP_RSN_IE_VERSION",
                                      "INVALID_RSN_IE_CAP",
                                      "802_1X_AUTH_FAILED",
                                      "CIPHER_SUITE_REJECTED",
                                      "BEACON_TIMEOUT",
                                      "NO_AP_FOUND",
                                      "AUTH_FAIL",
                                      "ASSOC_FAIL",
                                      "HANDSHAKE_TIMEOUT",
                                      "CONNECTION_FAIL"};
#define reason2str(r) (((r) > 176) ? system_event_reasons[(r)-176] : system_event_reasons[(r)-1])
#endif
// callback
auto CWiFiESP::eventCallback(wifiesp_event_t *event) -> esp_err_t {
    static bool firstConnect = true;

    if (event->eventId < WIFIESP_EVENT_MAX) {
        log_d("WiFiESP Event: %d - %s", event->eventId, wifiespEventNames[event->eventId]);
    }
    if (event->eventId == WIFIESP_EVENT_WIFI_SCAN_DONE) {
        scanDoneCb();

    } else if (event->eventId == WIFIESP_EVENT_WIFI_STA_START) {
        setStatus(WIFIESP_STATUS_DISCONNECTED);
        setEventStatusBits(WIFIESP_EVENTSTATUSBIT_STA_STARTED);
        if (esp_wifi_set_ps(_sleepEnabled) != ESP_OK) {
            log_e("esp_wifi_set_ps failed");
        }
    } else if (event->eventId == WIFIESP_EVENT_WIFI_STA_STOP) {
        setStatus(WIFIESP_STATUS_NO_WIFI);
        clearEventStatusBits(
            WIFIESP_EVENTSTATUSBIT_STA_STARTED | WIFIESP_EVENTSTATUSBIT_STA_CONNECTED |
            WIFIESP_EVENTSTATUSBIT_STA_HAS_IP | WIFIESP_EVENTSTATUSBIT_STA_HAS_IP6);
    } else if (event->eventId == WIFIESP_EVENT_WIFI_STA_CONNECTED) {
        setStatus(WIFIESP_STATUS_IDLE_STATUS);
        setEventStatusBits(WIFIESP_EVENTSTATUSBIT_STA_CONNECTED);

        // esp_netif_create_ip6_linklocal(_netIfs[ESP_IF_WIFI_STA]);
    } else if (event->eventId == WIFIESP_EVENT_WIFI_STA_DISCONNECTED) {
        uint8_t reason = event->eventInfo.wifiStaDisconnected.reason;
        log_w("Reason: %u - %s", reason, reason2str(reason));
        if (reason == WIFI_REASON_NO_AP_FOUND) {
            setStatus(WIFIESP_STATUS_NO_SSID_AVAIL);
        } else if ((reason == WIFI_REASON_AUTH_FAIL) && !firstConnect) {
            setStatus(WIFIESP_STATUS_CONNECT_FAILED);
        } else if (reason == WIFI_REASON_BEACON_TIMEOUT ||
                   reason == WIFI_REASON_HANDSHAKE_TIMEOUT) {
            setStatus(WIFIESP_STATUS_CONNECTION_LOST);
        } else if (reason == WIFI_REASON_AUTH_EXPIRE) {

        } else {
            setStatus(WIFIESP_STATUS_DISCONNECTED);
        }
        clearEventStatusBits(WIFIESP_EVENTSTATUSBIT_STA_CONNECTED |
                             WIFIESP_EVENTSTATUSBIT_STA_HAS_IP |
                             WIFIESP_EVENTSTATUSBIT_STA_HAS_IP6);
        if (firstConnect &&
            ((reason == WIFI_REASON_AUTH_EXPIRE) || (reason >= WIFI_REASON_BEACON_TIMEOUT))) {
            log_d("WiFi Reconnect Running");
            wiFiEsp.disconnect();
            wiFiEsp.begin();
            firstConnect = false;
        } else if (wiFiEsp.getAutoReconnect()) {
            if ((reason == WIFI_REASON_AUTH_EXPIRE) ||
                (reason >= WIFI_REASON_BEACON_TIMEOUT && reason != WIFI_REASON_AUTH_FAIL)) {
                log_d("WiFi AutoReconnect Running");
                wiFiEsp.disconnect();
                wiFiEsp.begin();
            }
        } else if (reason == WIFI_REASON_ASSOC_FAIL) {
            setStatus(WIFIESP_STATUS_CONNECT_FAILED);
        }
    } else if (event->eventId == WIFIESP_EVENT_WIFI_STA_GOT_IP) {
#if ARDUHAL_LOG_LEVEL >= ARDUHAL_LOG_LEVEL_DEBUG
        auto *ip = (uint8_t *)&(event->eventInfo.gotIp.ip_info.ip.addr);
        auto *mask = (uint8_t *)&(event->eventInfo.gotIp.ip_info.netmask.addr);
        auto *gw = (uint8_t *)&(event->eventInfo.gotIp.ip_info.gw.addr);
        log_d("STA IP: %u.%u.%u.%u, MASK: %u.%u.%u.%u, GW: %u.%u.%u.%u", ip[0], ip[1], ip[2], ip[3],
              mask[0], mask[1], mask[2], mask[3], gw[0], gw[1], gw[2], gw[3]);
#endif
        setStatus(WIFIESP_STATUS_CONNECTED);
        setEventStatusBits(WIFIESP_EVENTSTATUSBIT_STA_HAS_IP |
                           WIFIESP_EVENTSTATUSBIT_STA_CONNECTED);
    } else if (event->eventId == WIFIESP_EVENT_WIFI_STA_LOST_IP) {
        setStatus(WIFIESP_STATUS_IDLE_STATUS);
        clearEventStatusBits(WIFIESP_EVENTSTATUSBIT_STA_HAS_IP);

    } else if (event->eventId == WIFIESP_EVENT_WIFI_AP_START) {
        setEventStatusBits(WIFIESP_EVENTSTATUSBIT_AP_STARTED);
    } else if (event->eventId == WIFIESP_EVENT_WIFI_AP_STOP) {
        clearEventStatusBits(WIFIESP_EVENTSTATUSBIT_AP_STARTED |
                             WIFIESP_EVENTSTATUSBIT_AP_HAS_CLIENT);
    } else if (event->eventId == WIFIESP_EVENT_WIFI_AP_STACONNECTED) {
        setEventStatusBits(WIFIESP_EVENTSTATUSBIT_AP_HAS_CLIENT);
    } else if (event->eventId == WIFIESP_EVENT_WIFI_AP_STADISCONNECTED) {
        wifi_sta_list_t clients;
        if (esp_wifi_ap_get_sta_list(&clients) != ESP_OK || !clients.num) {
            clearEventStatusBits(WIFIESP_EVENTSTATUSBIT_AP_HAS_CLIENT);
        }

    } else if (event->eventId == WIFIESP_EVENT_ETH_START) {
        setEventStatusBits(WIFIESP_EVENTSTATUSBIT_ETH_STARTED);
    } else if (event->eventId == WIFIESP_EVENT_ETH_STOP) {
        clearEventStatusBits(
            WIFIESP_EVENTSTATUSBIT_ETH_STARTED | WIFIESP_EVENTSTATUSBIT_ETH_CONNECTED |
            WIFIESP_EVENTSTATUSBIT_ETH_HAS_IP | WIFIESP_EVENTSTATUSBIT_ETH_HAS_IP6);
    } else if (event->eventId == WIFIESP_EVENT_ETH_CONNECTED) {
        setEventStatusBits(WIFIESP_EVENTSTATUSBIT_ETH_CONNECTED);
    } else if (event->eventId == WIFIESP_EVENT_ETH_DISCONNECTED) {
        clearEventStatusBits(WIFIESP_EVENTSTATUSBIT_ETH_CONNECTED |
                             WIFIESP_EVENTSTATUSBIT_ETH_HAS_IP |
                             WIFIESP_EVENTSTATUSBIT_ETH_HAS_IP6);
    } else if (event->eventId == WIFIESP_EVENT_ETH_GOT_IP) {
#if ARDUHAL_LOG_LEVEL >= ARDUHAL_LOG_LEVEL_DEBUG
        auto *ip = (uint8_t *)&(event->eventInfo.gotIp.ip_info.ip.addr);
        auto *mask = (uint8_t *)&(event->eventInfo.gotIp.ip_info.netmask.addr);
        auto *gw = (uint8_t *)&(event->eventInfo.gotIp.ip_info.gw.addr);
        log_d("ETH IP: %u.%u.%u.%u, MASK: %u.%u.%u.%u, GW: %u.%u.%u.%u", ip[0], ip[1], ip[2], ip[3],
              mask[0], mask[1], mask[2], mask[3], gw[0], gw[1], gw[2], gw[3]);
#endif
        setEventStatusBits(WIFIESP_EVENTSTATUSBIT_ETH_CONNECTED |
                           WIFIESP_EVENTSTATUSBIT_ETH_HAS_IP);

    } else if (event->eventId == WIFIESP_EVENT_WIFI_STA_GOT_IP6) {
        setEventStatusBits(WIFIESP_EVENTSTATUSBIT_STA_CONNECTED |
                           WIFIESP_EVENTSTATUSBIT_STA_HAS_IP6);
    } else if (event->eventId == WIFIESP_EVENT_WIFI_AP_GOT_IP6) {
        setEventStatusBits(WIFIESP_EVENTSTATUSBIT_AP_HAS_IP6);
    } else if (event->eventId == WIFIESP_EVENT_ETH_GOT_IP6) {
        setEventStatusBits(WIFIESP_EVENTSTATUSBIT_ETH_CONNECTED |
                           WIFIESP_EVENTSTATUSBIT_ETH_HAS_IP6);
    } else if (event->eventId == WIFIESP_EVENT_SC_GOT_SSID_PSWD) {
        wiFiEsp.begin((const char *)event->eventInfo.scGotSsidPswd.ssid,
                      (const char *)event->eventInfo.scGotSsidPswd.password, 0,
                      ((event->eventInfo.scGotSsidPswd.bssid_set == true)
                           ? event->eventInfo.scGotSsidPswd.bssid
                           : nullptr));
    }
    // else if(event->event_id == WIFIESP_EVENT_SC_SEND_ACK_DONE) {
    //    esp_smartconfig_stop();
    //    _smartConfigDone = true;
    //}

    for (auto entry : _eventCBList) {
        if (entry.cb || entry.fcb || entry.scb) {
            if (entry.event == (wifiesp_event_id_t)event->eventId ||
                entry.event == WIFIESP_EVENT_MAX) {
                if (entry.cb) {
                    entry.cb((wifiesp_event_id_t)event->eventId);
                } else if (entry.fcb) {
                    entry.fcb((wifiesp_event_id_t)event->eventId,
                              (wifiesp_event_info_t)event->eventInfo);
                } else {
                    entry.scb(event);
                }
            }
        }
    }
    return ESP_OK;
}

//______________________________________________________________________________
//------------------------------------------------------------------------------
/*!
    \fn     onEvent

    \brief  set callback function
    \param  cbEvent - callback for event
    \param  event - event for callback
    \return wifi_event_id_t - id
*/
//______________________________________________________________________________
//------------------------------------------------------------------------------
auto CWiFiESP::onEvent(wifiesp_event_cb_t cbEvent, wifiesp_event_id_t event) -> wifi_event_id_t {
    if (!cbEvent) {
        return 0;
    }
    wifiesp_event_cb_tList_t newEventHandler;
    newEventHandler.cb = cbEvent;
    newEventHandler.fcb = nullptr;
    newEventHandler.scb = nullptr;
    newEventHandler.event = event;
    _eventCBList.push_back(newEventHandler);
    return newEventHandler.id;
}

//______________________________________________________________________________
//------------------------------------------------------------------------------
/*!
    \fn     onEvent

    \brief  set callback function
    \param  cbEvent - callback for event
    \param  event - event for callback
    \return wifi_event_id_t - id
*/
//______________________________________________________________________________
//------------------------------------------------------------------------------
auto CWiFiESP::onEvent(const wifiesp_event_func_cb_t &cbEvent, wifiesp_event_id_t event)
    -> wifi_event_id_t {
    if (!cbEvent) {
        return 0;
    }
    wifiesp_event_cb_tList_t newEventHandler;
    newEventHandler.cb = nullptr;
    newEventHandler.fcb = cbEvent;
    newEventHandler.scb = nullptr;
    newEventHandler.event = event;
    _eventCBList.push_back(newEventHandler);
    return newEventHandler.id;
}

//______________________________________________________________________________
//------------------------------------------------------------------------------
/*!
    \fn     onEvent

    \brief  set callback function
    \param  cbEvent - callback for event
    \param  event - event for callback
    \return wifi_event_id_t - id
*/
//______________________________________________________________________________
//------------------------------------------------------------------------------
auto CWiFiESP::onEvent(wifiesp_event_cb_sys_t cbEvent, wifiesp_event_id_t event)
    -> wifi_event_id_t {
    if (!cbEvent) {
        return 0;
    }
    wifiesp_event_cb_tList_t newEventHandler;
    newEventHandler.cb = nullptr;
    newEventHandler.fcb = nullptr;
    newEventHandler.scb = cbEvent;
    newEventHandler.event = event;
    _eventCBList.push_back(newEventHandler);
    return newEventHandler.id;
}

//______________________________________________________________________________
//------------------------------------------------------------------------------
/*!
    \fn     removeEvent

    \brief  removes a callback form event handler
    \param  cbEvent - callback for event
    \param  event - event optional filter (WIFI_EVENT_MAX is all events)
    \return void
*/
//______________________________________________________________________________
//------------------------------------------------------------------------------
void CWiFiESP::removeEvent(wifiesp_event_cb_t cbEvent, wifiesp_event_id_t event) {
    if (!cbEvent) {
        return;
    }

    for (uint32_t i = 0; i < _eventCBList.size(); i++) {
        wifiesp_event_cb_tList_t entry = _eventCBList[i];
        if (entry.cb == cbEvent && entry.event == event) {
            _eventCBList.erase(_eventCBList.begin() + i);
        }
    }
}

//______________________________________________________________________________
//------------------------------------------------------------------------------
/*!
    \fn     removeEvent

    \brief  removes a callback form event handler
    \param  cbEvent - callback for event
    \param  event - event optional filter (WIFI_EVENT_MAX is all events)
    \return void
*/
//______________________________________________________________________________
//------------------------------------------------------------------------------
void CWiFiESP::removeEvent(wifiesp_event_cb_sys_t cbEvent, wifiesp_event_id_t event) {
    if (!cbEvent) {
        return;
    }

    for (uint32_t i = 0; i < _eventCBList.size(); i++) {
        wifiesp_event_cb_tList_t entry = _eventCBList[i];
        if (entry.scb == cbEvent && entry.event == event) {
            _eventCBList.erase(_eventCBList.begin() + i);
        }
    }
}

//______________________________________________________________________________
//------------------------------------------------------------------------------
/*!
    \fn     removeEvent

    \brief  removes a callback form event handler
    \param  cbEvent - callback for event
    \param  event - event optional filter (WIFI_EVENT_MAX is all events)
    \return void
*/
//______________________________________________________________________________
//------------------------------------------------------------------------------
void CWiFiESP::removeEvent(wifi_event_id_t id) {
    for (uint32_t i = 0; i < _eventCBList.size(); i++) {
        wifiesp_event_cb_tList_t entry = _eventCBList[i];
        if (entry.id == id) {
            _eventCBList.erase(_eventCBList.begin() + i);
        }
    }
}

//-----------------------------------------------------------------------------
// PRIVATE
//-----------------------------------------------------------------------------

//______________________________________________________________________________
//------------------------------------------------------------------------------
/*!
    \fn     tcpipInit

    \brief  Init the TCPIP setups
    \param  N/A
    \return bool - true=ok
*/
//______________________________________________________________________________
//------------------------------------------------------------------------------
auto CWiFiESP::tcpipInit() -> bool {
    static bool initialized = false;
    if (!initialized) {
        initialized = true;
#if CONFIG_IDF_TARGET_ESP32
        uint8_t mac[8];
        if (esp_efuse_mac_get_default(mac) == ESP_OK) {
            esp_base_mac_addr_set(mac);
        }
#endif
        initialized = esp_netif_init() == ESP_OK;
        if (initialized) {
            initialized = WiFiEspTaskStartNetworkEventTask();
        } else {
            log_e("esp_netif_init failed!");
        }
    }
    return initialized;
}

//______________________________________________________________________________
//------------------------------------------------------------------------------
/*!
    \fn     wifiLowLevelInit

    \brief  Init the low level
    \param  persistent
    \return bool - true=ok
*/
//______________________________________________________________________________
//------------------------------------------------------------------------------
// Temporary fix to ensure that CDC+JTAG stay on on ESP32-C3
#if CONFIG_IDF_TARGET_ESP32C3
extern "C" void phy_bbpll_en_usb(bool en);
#endif
auto CWiFiESP::wifiLowLevelInit(bool persistent) -> bool {
    if (!lowLevelInitDone_) {
        lowLevelInitDone_ = true;
        if (!tcpipInit()) {
            lowLevelInitDone_ = false;
            return lowLevelInitDone_;
        }
        if (_netIfs[ESP_IF_WIFI_AP] == nullptr) {
            _netIfs[ESP_IF_WIFI_AP] = esp_netif_create_default_wifi_ap();
        }
        if (_netIfs[ESP_IF_WIFI_STA] == nullptr) {
            _netIfs[ESP_IF_WIFI_STA] = esp_netif_create_default_wifi_sta();
        }

        wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();

        // Do not use Static Buffers
        cfg.static_tx_buf_num = 0;
        cfg.dynamic_tx_buf_num = 32;
        cfg.tx_buf_type = 1;
        cfg.cache_tx_buf_num = 1; // can't be zero!
        cfg.static_rx_buf_num = 4;
        cfg.dynamic_rx_buf_num = 32;

        esp_err_t err = esp_wifi_init(&cfg);
        if (err) {
            log_e("esp_wifi_init %d", err);
            lowLevelInitDone_ = false;
            return lowLevelInitDone_;
        }
// Temporary fix to ensure that CDC+JTAG stay on on ESP32-C3
#if CONFIG_IDF_TARGET_ESP32C3
        phy_bbpll_en_usb(true);
#endif
        if (!persistent) {
            lowLevelInitDone_ = esp_wifi_set_storage(WIFI_STORAGE_RAM) == ESP_OK;
        }
        if (lowLevelInitDone_) {
            wifiesp_event_t wifiespEvent;
            wifiespEvent.eventId = WIFIESP_EVENT_WIFI_READY;
            WiFiEspTaskPostEvent(&wifiespEvent);
        }
    }
    return lowLevelInitDone_;
}

//______________________________________________________________________________
//------------------------------------------------------------------------------
/*!
    \fn     wifiLowLevelDeinit

    \brief  De-init the low level
    \param  N/A
    \return bool - true=ok
*/
//______________________________________________________________________________
//------------------------------------------------------------------------------
auto CWiFiESP::wifiLowLevelDeinit() -> bool {
    if (lowLevelInitDone_) {
        lowLevelInitDone_ = !(esp_wifi_deinit() == ESP_OK);
    }
    return !lowLevelInitDone_;
}

//______________________________________________________________________________
//------------------------------------------------------------------------------
/*!
    \fn     espWiFiStart

    \brief  Start the low level ESP Wifi
    \param  N/A
    \return bool - true=ok
*/
//______________________________________________________________________________
//------------------------------------------------------------------------------
auto CWiFiESP::espWiFiStart() -> bool {
    if (espWifiStarted_) {
        return true;
    }
    espWifiStarted_ = true;
    esp_err_t err = esp_wifi_start();
    if (err != ESP_OK) {
        espWifiStarted_ = false;
        log_e("esp_wifi_start %d", err);
        return espWifiStarted_;
    }
    return espWifiStarted_;
}

//______________________________________________________________________________
//------------------------------------------------------------------------------
/*!
    \fn     espWiFiStop

    \brief  Stop the low level ESP Wifi
    \param  N/A
    \return bool - true=ok
*/
//______________________________________________________________________________
//------------------------------------------------------------------------------
auto CWiFiESP::espWiFiStop() -> bool {
    esp_err_t err;
    if (!espWifiStarted_) {
        return true;
    }
    espWifiStarted_ = false;
    err = esp_wifi_stop();
    if (err) {
        log_e("Could not stop WiFi! %d", err);
        espWifiStarted_ = true;
        return false;
    }
    return wifiLowLevelDeinit();
}

//______________________________________________________________________________
//------------------------------------------------------------------------------
/*!
    \fn     EventStatusBits Get,Set,Wait

    \brief  Interact with the event status bits
    \param  N/A
    \return void
*/
//______________________________________________________________________________
//------------------------------------------------------------------------------
auto CWiFiESP::setEventStatusBits(int bits) -> int { return WiFiEspTaskSetEventStatusBits(bits); }

auto CWiFiESP::clearEventStatusBits(int bits) -> int {
    return WiFiEspTaskClearEventStatusBits(bits);
}

auto CWiFiESP::getEventStatusBits() -> int { return WiFiEspTaskGetEventStatusBits(); }

auto CWiFiESP::waitEventStatusBits(int bits, uint32_t timeoutMs) -> int {
    return WiFiEspTaskWaitEventStatusBits(bits, // The bits within the event group to wait for.
                                          timeoutMs / portTICK_PERIOD_MS) &
           bits; // Wait a maximum of 100ms for either bit to be set.
}

//______________________________________________________________________________
//------------------------------------------------------------------------------
/*!
    \fn     netif_interface

    \brief  Interact with the network interface settings
    \param  N/A
    \return void
*/
//______________________________________________________________________________
//------------------------------------------------------------------------------
auto CWiFiESP::getEspNetifInterface(esp_netif_t *espNetif) -> esp_interface_t {
    for (int i = 0; i < ESP_IF_MAX; i++) {
        if (_netIfs[i] != nullptr && _netIfs[i] == espNetif) {
            return (esp_interface_t)i;
        }
    }
    return ESP_IF_MAX;
}

void CWiFiESP::addEspInterfaceNetif(esp_interface_t interface, esp_netif_t *espNetif) {
    if (interface < ESP_IF_MAX) {
        _netIfs[interface] = espNetif;
    }
}

auto CWiFiESP::getEspInterfaceNetif(esp_interface_t interface) -> esp_netif_t * {
    if (interface < ESP_IF_MAX) {
        return _netIfs[interface];
    }
    return nullptr;
}

auto CWiFiESP::setEspInterfaceHostname(esp_interface_t interface, const char *hostname)
    -> esp_err_t {
    if (interface < ESP_IF_MAX) {
        return esp_netif_set_hostname(_netIfs[interface], hostname);
    }
    return ESP_FAIL;
}

auto CWiFiESP::setEspInterfaceIp(esp_interface_t interface, IPAddress localIp,
                                 const IPAddress &gateway, IPAddress subnet) -> esp_err_t {
    esp_netif_t *espNetif = _netIfs[interface];
    esp_netif_dhcp_status_t status = ESP_NETIF_DHCP_INIT;
    esp_netif_ip_info_t info;
    info.ip.addr = static_cast<uint32_t>(localIp);
    info.gw.addr = static_cast<uint32_t>(gateway);
    info.netmask.addr = static_cast<uint32_t>(subnet);

    log_v("Configuring %s static IP: " IPSTR ", MASK: " IPSTR ", GW: " IPSTR,
          interface == ESP_IF_WIFI_STA  ? "Station"
          : interface == ESP_IF_WIFI_AP ? "SoftAP"
                                        : "Ethernet",
          IP2STR(&info.ip), IP2STR(&info.netmask), IP2STR(&info.gw));

    esp_err_t err = ESP_OK;
    if (interface != ESP_IF_WIFI_AP) {
        err = esp_netif_dhcpc_get_status(espNetif, &status);
        if (err) {
            log_e("DHCPC Get Status Failed! 0x%04x", err);
            return err;
        }
        err = esp_netif_dhcpc_stop(espNetif);
        if (err && err != ESP_ERR_ESP_NETIF_DHCP_ALREADY_STOPPED) {
            log_e("DHCPC Stop Failed! 0x%04x", err);
            return err;
        }
        err = esp_netif_set_ip_info(espNetif, &info);
        if (err) {
            log_e("Netif Set IP Failed! 0x%04x", err);
            return err;
        }
        if (info.ip.addr == 0) {
            err = esp_netif_dhcpc_start(espNetif);
            if (err) {
                log_e("DHCPC Start Failed! 0x%04x", err);
                return err;
            }
        }
    } else {
        err = esp_netif_dhcps_get_status(espNetif, &status);
        if (err) {
            log_e("DHCPS Get Status Failed! 0x%04x", err);
            return err;
        }
        err = esp_netif_dhcps_stop(espNetif);
        if (err && err != ESP_ERR_ESP_NETIF_DHCP_ALREADY_STOPPED) {
            log_e("DHCPS Stop Failed! 0x%04x", err);
            return err;
        }
        err = esp_netif_set_ip_info(espNetif, &info);
        if (err) {
            log_e("Netif Set IP Failed! 0x%04x", err);
            return err;
        }

        dhcps_lease_t lease;
        lease.enable = true;
        auto dhcpIpaddr = static_cast<uint32_t>(localIp);
        // prevents DHCP lease range to overflow subnet/24 range
        // there will be 11 addresses for DHCP to lease
        auto leaseStart = (uint8_t)(~subnet[3] - 12);
        if ((localIp[3]) < leaseStart) {
            lease.start_ip.addr = dhcpIpaddr + (1 << 24);
            lease.end_ip.addr = dhcpIpaddr + (11 << 24);
        } else {
            // make range stay in the begining of the netmask range
            dhcpIpaddr = (dhcpIpaddr & 0x00FFFFFF);
            lease.start_ip.addr = dhcpIpaddr + (1 << 24);
            lease.end_ip.addr = dhcpIpaddr + (11 << 24);
        }
        log_v("DHCP Server Range: %s to %s", IPAddress(lease.start_ip.addr).toString().c_str(),
              IPAddress(lease.end_ip.addr).toString().c_str());
        err = tcpip_adapter_dhcps_option((tcpip_adapter_dhcp_option_mode_t)TCPIP_ADAPTER_OP_SET,
                                         (tcpip_adapter_dhcp_option_id_t)REQUESTED_IP_ADDRESS,
                                         (void *)&lease, sizeof(dhcps_lease_t));
        if (err) {
            log_e("DHCPS Set Lease Failed! 0x%04x", err);
            return err;
        }

        err = esp_netif_dhcps_start(espNetif);
        if (err) {
            log_e("DHCPS Start Failed! 0x%04x", err);
            return err;
        }
    }
    return err;
}

auto CWiFiESP::setEspInterfaceDns(esp_interface_t interface, const IPAddress &mainDns,
                                  const IPAddress &backupDns, const IPAddress &fallbackDns)
    -> esp_err_t {
    esp_netif_t *espNetif = _netIfs[interface];
    esp_netif_dns_info_t dns;
    dns.ip.type = ESP_IPADDR_TYPE_V4;
    dns.ip.u_addr.ip4.addr = static_cast<uint32_t>(mainDns);
    if (dns.ip.u_addr.ip4.addr &&
        esp_netif_set_dns_info(espNetif, ESP_NETIF_DNS_MAIN, &dns) != ESP_OK) {
        log_e("Set Main DNS Failed!");
        return ESP_FAIL;
    }
    if (interface != ESP_IF_WIFI_AP) {
        dns.ip.u_addr.ip4.addr = static_cast<uint32_t>(backupDns);
        if (dns.ip.u_addr.ip4.addr &&
            esp_netif_set_dns_info(espNetif, ESP_NETIF_DNS_BACKUP, &dns) != ESP_OK) {
            log_e("Set Backup DNS Failed!");
            return ESP_FAIL;
        }
        dns.ip.u_addr.ip4.addr = static_cast<uint32_t>(fallbackDns);
        if (dns.ip.u_addr.ip4.addr &&
            esp_netif_set_dns_info(espNetif, ESP_NETIF_DNS_FALLBACK, &dns) != ESP_OK) {
            log_e("Set Fallback DNS Failed!");
            return ESP_FAIL;
        }
    }
    return ESP_OK;
}

auto CWiFiESP::getEspNetifHostname() -> const char * {
    if (_default_hostname[0] == 0) {
        uint8_t ethMac[6];
        esp_wifi_get_mac((wifi_interface_t)WIFI_IF_STA, ethMac);
        snprintf(_default_hostname, 32, "%s%02X%02X%02X", CONFIG_IDF_TARGET "-", ethMac[3],
                 ethMac[4], ethMac[5]);
    }
    return (const char *)_default_hostname;
}

void CWiFiESP::setEspNetifHostname(const char *name) {
    if (name) {
        snprintf(_default_hostname, 32, "%s", name);
    }
}

//______________________________________________________________________________
//------------------------------------------------------------------------------
/*!
    \fn     _wifi_strncpy

    \brief  Copy the wifi string
    \param  dst - destination
        \param  src - source
        \dst_len - detination length
    \return length copied
*/
//______________________________________________________________________________
//------------------------------------------------------------------------------
auto CWiFiESP::wifiStrncpy(char *dst, const char *src, size_t dstLen) -> size_t {
    if (!dst || !src || !dstLen) {
        return 0;
    }
    size_t src_len = strlen(src);
    if (src_len >= dstLen) {
        src_len = dstLen;
    } else {
        src_len += 1;
    }
    strcpy(dst, src);
    return src_len;
}

//______________________________________________________________________________
//------------------------------------------------------------------------------
/*!
    \fn     sta_config_equal

    \brief  compare two STA configurations
    \param lhs station_config
    \param rhs station_config
    \return bool - true=equal
*/
//______________________________________________________________________________
//------------------------------------------------------------------------------
auto CWiFiESP::staConfigEqual(const wifi_config_t &lhs, const wifi_config_t &rhs) -> bool {
    if (memcmp(&lhs, &rhs, sizeof(wifi_config_t)) != 0) {
        return false;
    }
    return true;
}

//______________________________________________________________________________
//------------------------------------------------------------------------------
/*!
    \fn     wifi_sta_config

    \brief  Set the STA Config
    \param  N/A
    \return void
*/
//______________________________________________________________________________
//------------------------------------------------------------------------------
void CWiFiESP::wifiStaConfig(wifi_config_t *wifiConfig, const char *ssid, const char *password,
                             const uint8_t *bssid, uint8_t channel, wifi_scan_method_t scanMethod,
                             wifi_sort_method_t sortMethod, uint16_t listenInterval,
                             bool pmfRequired) {
    wifiConfig->sta.channel = channel;
    wifiConfig->sta.listen_interval = listenInterval;
    wifiConfig->sta.scan_method = scanMethod; // WIFI_ALL_CHANNEL_SCAN or WIFI_FAST_SCAN
    wifiConfig->sta.sort_method =
        sortMethod; // WIFI_CONNECT_AP_BY_SIGNAL or WIFI_CONNECT_AP_BY_SECURITY
    wifiConfig->sta.threshold.rssi = -127;
    wifiConfig->sta.pmf_cfg.capable = true;
    wifiConfig->sta.pmf_cfg.required = pmfRequired;
    wifiConfig->sta.bssid_set = false;
    memset(wifiConfig->sta.bssid, 0, 6);
    wifiConfig->sta.threshold.authmode = WIFI_AUTH_OPEN;
    wifiConfig->sta.ssid[0] = 0;
    wifiConfig->sta.password[0] = 0;
    if (ssid != nullptr && ssid[0] != 0) {
        wifiStrncpy((char *)wifiConfig->sta.ssid, ssid, 32);
        if (password != nullptr && password[0] != 0) {
            wifiConfig->sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;
            wifiStrncpy((char *)wifiConfig->sta.password, password, 64);
        }
        if (bssid != nullptr) {
            wifiConfig->sta.bssid_set = true;
            memcpy(wifiConfig->sta.bssid, bssid, 6);
        }
    }
}
