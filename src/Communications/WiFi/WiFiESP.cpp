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

#include <string.h>
#include <string>

#include "esp_event.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "lwip/err.h"
#include "lwip/sys.h"
#include "nvs_flash.h"

//-----------------------------------------------------------------------------
// Object
//-----------------------------------------------------------------------------
// Create Object
CWiFiESP wiFiEsp;

// Tag for the logging
static const char *TAG = "WiDiESP";

/*
static void print_auth_mode(int authmode)
{
    switch (authmode) {
    case WIFI_AUTH_OPEN:
        ESP_LOGI(TAG, "Authmode \tWIFI_AUTH_OPEN");
        break;
    case WIFI_AUTH_WEP:
        ESP_LOGI(TAG, "Authmode \tWIFI_AUTH_WEP");
        break;
    case WIFI_AUTH_WPA_PSK:
        ESP_LOGI(TAG, "Authmode \tWIFI_AUTH_WPA_PSK");
        break;
    case WIFI_AUTH_WPA2_PSK:
        ESP_LOGI(TAG, "Authmode \tWIFI_AUTH_WPA2_PSK");
        break;
    case WIFI_AUTH_WPA_WPA2_PSK:
        ESP_LOGI(TAG, "Authmode \tWIFI_AUTH_WPA_WPA2_PSK");
        break;
    case WIFI_AUTH_WPA2_ENTERPRISE:
        ESP_LOGI(TAG, "Authmode \tWIFI_AUTH_WPA2_ENTERPRISE");
        break;
    case WIFI_AUTH_WPA3_PSK:
        ESP_LOGI(TAG, "Authmode \tWIFI_AUTH_WPA3_PSK");
        break;
    case WIFI_AUTH_WPA2_WPA3_PSK:
        ESP_LOGI(TAG, "Authmode \tWIFI_AUTH_WPA2_WPA3_PSK");
        break;
    default:
        ESP_LOGI(TAG, "Authmode \tWIFI_AUTH_UNKNOWN");
        break;
    }
}

static void print_cipher_type(int pairwise_cipher, int group_cipher)
{
    switch (pairwise_cipher) {
    case WIFI_CIPHER_TYPE_NONE:
        ESP_LOGI(TAG, "Pairwise Cipher \tWIFI_CIPHER_TYPE_NONE");
        break;
    case WIFI_CIPHER_TYPE_WEP40:
        ESP_LOGI(TAG, "Pairwise Cipher \tWIFI_CIPHER_TYPE_WEP40");
        break;
    case WIFI_CIPHER_TYPE_WEP104:
        ESP_LOGI(TAG, "Pairwise Cipher \tWIFI_CIPHER_TYPE_WEP104");
        break;
    case WIFI_CIPHER_TYPE_TKIP:
        ESP_LOGI(TAG, "Pairwise Cipher \tWIFI_CIPHER_TYPE_TKIP");
        break;
    case WIFI_CIPHER_TYPE_CCMP:
        ESP_LOGI(TAG, "Pairwise Cipher \tWIFI_CIPHER_TYPE_CCMP");
        break;
    case WIFI_CIPHER_TYPE_TKIP_CCMP:
        ESP_LOGI(TAG, "Pairwise Cipher \tWIFI_CIPHER_TYPE_TKIP_CCMP");
        break;
    default:
        ESP_LOGI(TAG, "Pairwise Cipher \tWIFI_CIPHER_TYPE_UNKNOWN");
        break;
    }

    switch (group_cipher) {
    case WIFI_CIPHER_TYPE_NONE:
        ESP_LOGI(TAG, "Group Cipher \tWIFI_CIPHER_TYPE_NONE");
        break;
    case WIFI_CIPHER_TYPE_WEP40:
        ESP_LOGI(TAG, "Group Cipher \tWIFI_CIPHER_TYPE_WEP40");
        break;
    case WIFI_CIPHER_TYPE_WEP104:
        ESP_LOGI(TAG, "Group Cipher \tWIFI_CIPHER_TYPE_WEP104");
        break;
    case WIFI_CIPHER_TYPE_TKIP:
        ESP_LOGI(TAG, "Group Cipher \tWIFI_CIPHER_TYPE_TKIP");
        break;
    case WIFI_CIPHER_TYPE_CCMP:
        ESP_LOGI(TAG, "Group Cipher \tWIFI_CIPHER_TYPE_CCMP");
        break;
    case WIFI_CIPHER_TYPE_TKIP_CCMP:
        ESP_LOGI(TAG, "Group Cipher \tWIFI_CIPHER_TYPE_TKIP_CCMP");
        break;
    default:
        ESP_LOGI(TAG, "Group Cipher \tWIFI_CIPHER_TYPE_UNKNOWN");
        break;
    }
}
*/

//______________________________________________________________________________
//------------------------------------------------------------------------------
/*!
    \fn     wifi_event_handler

    \brief  Event Callback handler
    \param  N/A
    \return N/A
*/
//______________________________________________________________________________
//------------------------------------------------------------------------------
static void event_handler(void *arg, esp_event_base_t event_base, int32_t event_id,
                          void *event_data) {
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        if (wiFiEsp.getAutoReconnect()) esp_wifi_connect();
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
        ESP_LOGI(TAG, "got ip: " IPSTR, IP2STR(&event->ip_info.ip));
        wiFiEsp.setConnected(true);
    }
}

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

    esp_err_t err;

    err = esp_wifi_set_mode(m);
    if (err) {
        ESP_LOGI(TAG, "Could not set mode! %d", err);
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
    wifi_mode_t mode;
    if (esp_wifi_get_mode(&mode) != ESP_OK) {
        ESP_LOGI(TAG, "WiFi not started");
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
    \fn     SSID

    \brief  Return the SSID discovered during the network scan.
    \param  i specify from which network item want to get the information
    \return string - ssid string of the specified item on the networks scanned list
*/
//______________________________________________________________________________
//------------------------------------------------------------------------------
auto CWiFiESP::ssid(uint8_t i) -> std::string {
    if (i >= _ap_count) {
        return {};
    }

    return {reinterpret_cast<const char *>(_ap_info[i].ssid)};
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
    if (i >= _ap_count) {
        return {};
    }

    return _ap_info[i].authmode;
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
    if (i >= _ap_count) {
        return {};
    }

    return _ap_info[i].rssi;
}

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
auto CWiFiESP::getHostname() -> const char * {
    if (_default_hostname[0] == 0) {
        uint8_t ethMac[6];
        esp_wifi_get_mac((wifi_interface_t)WIFI_IF_STA, ethMac);
        snprintf(_default_hostname, 32, "%s%02X%02X%02X", CONFIG_IDF_TARGET "-", ethMac[3],
                 ethMac[4], ethMac[5]);
    }
    return (const char *)_default_hostname;
}

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
auto CWiFiESP::setHostname(const char *hostname) -> void {
    if (hostname) {
        snprintf(_default_hostname, 32, "%s", hostname);
    }
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
                     const uint8_t *bssid, bool connect) -> void {

    if (!ssid || *ssid == 0x00 || strlen(ssid) > 32) {
        ESP_LOGI(TAG, "SSID too long or missing!");
        _status = WIFIESP_STATUS_CONNECT_FAILED;
        return;
    }

    if (passphrase && strlen(passphrase) > 64) {
        ESP_LOGI(TAG, "passphrase too long!");
        _status = WIFIESP_STATUS_CONNECT_FAILED;
        return;
    }

    // TODO SAVE THE CHANNEL ETC

    strcpy(_default_hostname, ssid);
    strcpy(_default_password, passphrase);

    if (connect) {
        begin();
    }
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
                     bool connect) -> void {
    begin((const char *)ssid, (const char *)passphrase, channel, bssid, connect);
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
auto CWiFiESP::begin() -> void {

    // TODO FROM EXAMPLE CODE _ BUT DOESNT EXIST TYPE
    // esp_pm_config_esp32s3_t pm_config = {
    //         .max_freq_mhz = CONFIG_EXAMPLE_MAX_CPU_FREQ_MHZ,
    //         .min_freq_mhz = CONFIG_EXAMPLE_MIN_CPU_FREQ_MHZ,
    //         .light_sleep_enable = true
    // };
    // ESP_ERROR_CHECK( esp_pm_configure(&pm_config) );

    // Already Done in Scan
    // ESP_ERROR_CHECK(esp_netif_init());
    // ESP_ERROR_CHECK(esp_event_loop_create_default());
    // esp_netif_t *sta_netif = esp_netif_create_default_wifi_sta();
    // assert(sta_netif);

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID,
                                                        &event_handler, NULL, NULL));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP,
                                                        &event_handler, NULL, NULL));

    wifi_config_t wifi_config = {
        .sta =
            {
                //.ssid = _default_hostname,
                //.password = _default_password,
                //.scan_method = WIFI_FAST_SCAN,
                //.bssid_set = 0 - whether set MAC address of target AP or not. Generally,
                // station_config.bssid_set needs to be 0; and it needs to be 1 only when users
                // need to check the MAC address of the AP. .bssid - MAC address of target AP
                //.channel = 0, - channel of target AP. Set to 1~13 to scan starting from the
                // specified channel before connecting to AP. If the channel of AP is unknown,
                // set it to 0. .sort_method = WIFI_CONNECT_AP_BY_SIGNAL - sort the connect AP in
                // the list by rssi or security mode .threshold - When scan_method is set to
                // WIFI_FAST_SCAN, only APs which have an auth mode that is more secure than the
                // selected auth mode and a signal stronger than the minimum RSSI will be used

                // listen interval may not work - dont see it in docs
                .listen_interval = 3, // 300ms
                                      // Interval for station to listen to beacon from AP. The unit
                                      // of listen interval is one beacon interval. For example, if
                                      // beacon interval is 100 ms and listen interval is 3, the
                                      // interval for station to listen to beacon is 300 ms.
            },
    };

    strcpy((char *)wifi_config.sta.ssid, _default_hostname);
    strcpy((char *)wifi_config.sta.password, _default_password);

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "esp_wifi_set_ps().");
    esp_wifi_set_ps(WIFI_PS_NONE);
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
    _autoReconnect = autoReconnect;
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
auto CWiFiESP::getAutoReconnect() -> bool { return _autoReconnect; }

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
    if (esp_wifi_disconnect() == ESP_OK) {
        return esp_wifi_connect() == ESP_OK;
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
    // wifiStaConfig(&conf);

    if (getMode() & WIFI_MODE_STA) {
        if (eraseap) {
            if (esp_wifi_set_config((wifi_interface_t)ESP_IF_WIFI_STA, &conf)) {
                ESP_LOGI(TAG, "clear config failed!");
            }
        }
        if (esp_wifi_disconnect()) {
            ESP_LOGI(TAG, "disconnect failed!");
            return false;
        }
        // if (wifioff) {
        //     return enableSTA(false);
        // }

        // note we are now disconnected
        _status = WIFIESP_STATUS_DISCONNECTED;

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
auto CWiFiESP::isConnected() -> bool { return (getStatus() == WIFIESP_STATUS_CONNECTED); }

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
auto CWiFiESP::localIP() -> wifiesp_ip4address {
    if (getMode() == WIFI_MODE_NULL) {
        return IPADDRESS_NONE;
    }
    esp_netif_ip_info_t ip;
    if (esp_netif_get_ip_info(_sta_netif, &ip) != ESP_OK) {
        ESP_LOGI(TAG, "Netif Get IP Failed!");
        return IPADDRESS_NONE;
    }
    return (wifiesp_ip4address)ip.ip;
}
