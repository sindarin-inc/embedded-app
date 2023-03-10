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
#include <esp_wifi.h>
#include <functional>
#include <string>

#include "Misc/Platform.hpp"
#include "WiFiESPDefs.hpp"
#include "esp_err.h"
#include "esp_event.h"
#include "esp_smartconfig.h"
#include "wifi_provisioning/manager.h"

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

    // Modes
    auto mode(wifi_mode_t) -> bool;
    auto getMode() -> wifi_mode_t;
    auto enableSTA(bool enable) -> bool;

    // Scan
    auto scanNetworks(bool async = false, bool showHidden = false, bool passive = false,
                      uint32_t maxMsPerChan = 300, uint8_t channel = 0) -> int16_t;
    static void scanDoneCb();
    auto scanComplete() -> int16_t;
    void scanDelete();
    // Scan Information
    auto getScanInfoByIndex(int i) -> void *;
    auto getNetworkInfo(uint8_t networkItem, std::string &ssid, uint8_t &encryptionType,
                        int32_t &rssi, uint8_t *&bssid, int32_t &channel) -> bool;
    auto ssid(uint8_t networkItem) -> std::string;
    auto encryptionType(uint8_t networkItem) -> wifi_auth_mode_t;
    auto rssi(uint8_t networkItem) -> int32_t;
    auto bssid(uint8_t networkItem) -> uint8_t *;
    auto bssiDstr(uint8_t networkItem) -> std::string;
    auto scanChannel(uint8_t networkItem) -> int32_t;

    // Sleep
    auto setSleep(bool enabled) -> bool;
    auto setSleep(wifi_ps_type_t sleepType) -> bool;
    auto getSleep() -> wifi_ps_type_t;

    // Power
    auto setTxPower(wifi_power_t power) -> bool;
    auto getTxPower() -> wifi_power_t;

    // Hostname
    auto getHostname() -> const char *;
    auto setHostname(const char *hostname) -> bool;
    auto hostname(const std::string &aHostname) -> bool { return setHostname(aHostname.c_str()); }

    // Start Wifi
    auto begin(const char *wpa2Ssid, wifiesp_wpa2_auth_method_t method,
               const char *wpa2Identity = nullptr, const char *wpa2Username = nullptr,
               const char *wpa2Password = nullptr, const char *caPem = nullptr,
               const char *clientCrt = nullptr, const char *clientKey = nullptr,
               int32_t channel = 0, const uint8_t *bssid = nullptr, bool connect = true)
        -> wifiesp_status_t;
    auto begin(const char *ssid, const char *passphrase = nullptr, int32_t channel = 0,
               const uint8_t *bssid = nullptr, bool connect = true) -> wifiesp_status_t;
    auto begin(char *ssid, char *passphrase = nullptr, int32_t channel = 0,
               const uint8_t *bssid = nullptr, bool connect = true) -> wifiesp_status_t;
    auto begin() -> wifiesp_status_t;

    // Auto Reconnect
    auto setAutoReconnect(bool autoReconnect) -> bool;
    auto getAutoReconnect() -> bool;

    // Connection
    auto reconnect() -> bool;
    auto disconnect(bool wifioff = false, bool eraseap = false) -> bool;
    auto isConnected() -> bool;
    auto localIP() -> wifiesp_ip4address;

    // STA Status
    static void setStatus(wifiesp_status_t status);
    static auto status() -> wifiesp_status_t;

    // Event Callback
    static auto eventCallback(wifiesp_event_t *event) -> esp_err_t;
    // Events
    auto onEvent(wifiesp_event_cb_t cbEvent, wifiesp_event_id_t event = WIFIESP_EVENT_MAX)
        -> wifi_event_id_t;
    auto onEvent(const wifiesp_event_func_cb_t &cbEvent,
                 wifiesp_event_id_t event = WIFIESP_EVENT_MAX) -> wifi_event_id_t;
    auto onEvent(wifiesp_event_cb_sys_t cbEvent, wifiesp_event_id_t event = WIFIESP_EVENT_MAX)
        -> wifi_event_id_t;
    void removeEvent(wifiesp_event_cb_t cbEvent, wifiesp_event_id_t event = WIFIESP_EVENT_MAX);
    void removeEvent(wifiesp_event_cb_sys_t cbEvent, wifiesp_event_id_t event = WIFIESP_EVENT_MAX);
    void removeEvent(wifi_event_id_t id);

    // Wait Status Bits
    static auto waitEventStatusBits(int bits, uint32_t timeoutMs) -> int;
    // Get Network Inferace
    static auto getEspNetifInterface(esp_netif_t *espNetif) -> esp_interface_t;

protected:
    //---------------------------------
    // Variables
    //---------------------------------

    // Init
    bool lowLevelInitDone_ = false;
    bool espWifiStarted_ = false;

    bool persistent_ = true;
    bool longRange_ = false;
    bool autoReconnect_ = true;

    //---------------------------------
    // Functions
    //---------------------------------

    // Init
    auto tcpipInit() -> bool;
    auto wifiLowLevelInit(bool persistent) -> bool;
    auto wifiLowLevelDeinit() -> bool;
    auto espWiFiStart() -> bool;
    auto espWiFiStop() -> bool;

    // Event Status Bits
    static auto setEventStatusBits(int bits) -> int;
    static auto clearEventStatusBits(int bits) -> int;
    static auto getEventStatusBits() -> int;

    // Network Interface
    void addEspInterfaceNetif(esp_interface_t interface, esp_netif_t *espNetif);
    auto getEspInterfaceNetif(esp_interface_t interface) -> esp_netif_t *;
    auto setEspInterfaceHostname(esp_interface_t interface, const char *hostname) -> esp_err_t;
    auto setEspInterfaceIp(esp_interface_t interface, wifiesp_ip4address localIp = IPADDRESS_NONE,
                           const wifiesp_ip4address &gateway = IPADDRESS_NONE,
                           wifiesp_ip4address subnet = IPADDRESS_NONE) -> esp_err_t;
    auto setEspInterfaceDns(esp_interface_t interface,
                            const wifiesp_ip4address &mainDns = IPADDRESS_NONE,
                            const wifiesp_ip4address &backupDns = IPADDRESS_NONE,
                            const wifiesp_ip4address &fallbackDns = IPADDRESS_NONE) -> esp_err_t;
    static auto getEspNetifHostname() -> const char *;
    static void setEspNetifHostname(const char *name);

    // Helper functions
    static auto wifiStrncpy(char *dst, const char *src, size_t dstLen) -> size_t;
    // STA Helpers
    static auto staConfigEqual(const wifi_config_t &lhs, const wifi_config_t &rhs) -> bool;
    static void wifiStaConfig(wifi_config_t *wifiConfig, const char *ssid = nullptr,
                              const char *password = nullptr, const uint8_t *bssid = nullptr,
                              uint8_t channel = 0,
                              wifi_scan_method_t scanMethod = WIFI_ALL_CHANNEL_SCAN,
                              wifi_sort_method_t sortMethod = WIFI_CONNECT_AP_BY_SIGNAL,
                              uint16_t listenInterval = 0, bool pmfRequired = false);
};

//-----------------------------------------------------------------------------
// Extern Object
//-----------------------------------------------------------------------------
// Global Object
extern CWiFiESP wiFiEsp;
