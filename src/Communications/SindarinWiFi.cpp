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

#include <string>

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

auto CSindarinWiFi::begin() -> esp_err_t {
    if (began_) {
        return ESP_OK;
    }

    SHOW_HEAP();

    esp_err_t err = esp_netif_init(); // ~5k
    if (err != ESP_OK) {
        log_e("esp_netif_init failed with error %s", esp_err_to_name(err));
        return err;
    }
    SHOW_HEAP();

    err = esp_event_loop_create_default(); // 4k
    if (err != ESP_OK) {
        log_e("esp_event_loop_create_default failed with error %s", esp_err_to_name(err));
        return err;
    }

    SHOW_HEAP();
    esp_netif_t *sta_netif = esp_netif_create_default_wifi_sta(); // 1k
    if (sta_netif == nullptr) {
        log_e("esp_netif_create_default_wifi_sta returned NULL") return ESP_ERR_NO_MEM;
    }

    SHOW_HEAP();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    cfg.static_tx_buf_num = 0;
    cfg.dynamic_tx_buf_num = 32;
    cfg.tx_buf_type = 1;
    cfg.cache_tx_buf_num = 1;
    cfg.static_rx_buf_num = 4;
    cfg.dynamic_rx_buf_num = 32;
    err = esp_wifi_init(&cfg); // 19k SRAM, 23k PSRAM
    if (err != ESP_OK) {
        log_e("esp_wifi_init failed with error %s", esp_err_to_name(err));
        return err;
    }
    SHOW_HEAP();

    err = esp_wifi_set_mode(WIFI_MODE_STA); // 0k
    if (err != ESP_OK) {
        log_e("esp_wifi_set_mode failed with error %s", esp_err_to_name(err));
        return err;
    }
    SHOW_HEAP();

    err = esp_wifi_start();
    if (err != ESP_OK) {
        log_e("esp_wifi_start failed with error %s", esp_err_to_name(err));
        return err;
    }
    SHOW_HEAP();

    began_ = true;
    return ESP_OK;
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
    if (esp_wifi_set_config(WIFI_IF_STA, conf) != ESP_OK) {
        log_e("STA enable failed!");
        return false;
    }

    if (esp_wifi_get_config(WIFI_IF_STA, conf) != ESP_OK) {
        log_e("Failed to get WiFi config");
        return false;
    } else {
        // We got the config - now check it
        if (strlen((const char *)conf->sta.ssid) == 0) {
            return false;
        }
        if (strlen((const char *)conf->sta.password) == 0) {
            log_w("Zero length WiFi password");
        }
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

auto CSindarinWiFi::scanNetworks(
    const std::function<void(wifi_ap_record_t *apRecords, int recordCount)> &completeCB)
    -> esp_err_t {

    // Set WiFi to station mode and disconnect from an AP if it was previously connected
    // TODO: Do we need this?
    // sindarinWiFi.setMode(WIFI_MODE_STA);
    // sindarinWiFi.disconnectWiFi();

    esp_err_t err = begin();
    if (err != ESP_OK) {
        log_e("begin failed with error %s", esp_err_to_name(err));
        return err;
    }

    // TODO: Use a non-blocking scan and send reusults back in realtime
    // Q: How do we get callbacks when a new network is found
    err = esp_wifi_scan_start(nullptr, true); // 3k PSRAM
    if (err != ESP_OK) {
        log_e("esp_wifi_scan_start failed with error %s", esp_err_to_name(err));
        return err;
    }
    SHOW_HEAP();

    uint16_t apNum = 0;
    err = esp_wifi_scan_get_ap_num(&apNum); // 0k
    if (err != ESP_OK) {
        log_e("esp_wifi_scan_get_ap_num failed with error %s", esp_err_to_name(err));
        return err;
    }
    SHOW_HEAP();

    auto apRecords =
        (wifi_ap_record_t *)heap_caps_malloc(sizeof(wifi_ap_record_t) * apNum, MALLOC_CAP_8BIT);

    err = esp_wifi_scan_get_ap_records(&apNum, apRecords);
    log_i("Total APs scanned = %u", apNum);

    completeCB(apRecords, apNum);

    heap_caps_free(apRecords);
    SHOW_HEAP();

    return ESP_OK;
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

    // If credentials are available, go ahead and attempt WiFi connection
    // wifi_config_t conf;
    // getWiFiCredentials(&conf);
    // log_i("Found WiFi credentials for %s. Attempting connection", (const char *)conf.sta.ssid);
    // wiFiEsp.setHostname(HOSTNAME);
    begin();

    // if (ssid.empty()) {
    //     wiFiEsp.begin();
    // } else {
    //     wiFiEsp.begin(ssid.c_str(), password.c_str());
    // }

    wifi_config_t wifi_config = {
        .sta =
            {
                // This setting limits auth modes to WPA2 and WPA3. We might want to do something
                // like this for security reasons.
                // .threshold.authmode = ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD,
                .sae_pwe_h2e = WPA3_SAE_PWE_BOTH,
            },
    };
    strncpy((char *)wifi_config.sta.ssid, ssid.c_str(), sizeof(wifi_config.sta.ssid));
    strncpy((char *)wifi_config.sta.password, password.c_str(), sizeof(wifi_config.sta.password));

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    int8_t power = 0;
    esp_wifi_get_max_tx_power(&power);
    log_d("WiFi TX Power was %d", power);
    esp_wifi_set_max_tx_power(power);
    esp_wifi_get_max_tx_power(&power);
    log_d("WiFi TX Power is now %d", power);

    // wiFiEsp.setAutoReconnect(true);
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
    auto startTime = xTaskGetTickCount();
    if (!connected()) {
        if (connectWiFiIfCredentialsExist()) {
            // Only wait if we had credentials
            // TODO: Would be nice to make this not a polling loop
            while (!connected() && (xTaskGetTickCount() - startTime) < timeoutMs) {
                vTaskDelay(200);
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
auto CSindarinWiFi::getLocalIp() -> wifiesp_ip4address { return wiFiEsp.localIP(); }

//______________________________________________________________________________
//------------------------------------------------------------------------------
/*!
    \fn     GetLocalIPStr

    \brief  Get the station interface IP address as a string
    \param  N/A
    \return IPAddress = 0 if not cennected
*/
//______________________________________________________________________________
//------------------------------------------------------------------------------
auto CSindarinWiFi::getLocalIpStr() -> std::string {
    esp_ip4_addr_t ip = (esp_ip4_addr_t)getLocalIp();
    char ipstr[16];
    sprintf(ipstr, IPSTR, IP2STR(&ip));
    return ipstr;
}

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
auto CSindarinWiFi::getStatus() -> wifiesp_status_t { return wiFiEsp.getStatus(); }

// //______________________________________________________________________________
// //------------------------------------------------------------------------------
// /*!
//     \fn     GetMode

//     \brief  Return Wifi mode
//     \param  N/A
//     \return wifi_mode_t
// */
// //______________________________________________________________________________
// //------------------------------------------------------------------------------
// auto CSindarinWiFi::getMode() -> wifi_mode_t { return wiFiEsp.getMode(); }

// //______________________________________________________________________________
// //------------------------------------------------------------------------------
// /*!
//     \fn     SetMode

//     \brief  Set a new mode
//     \param  wifi_mode_t - mode to set
//     \return bool - true= mode set
// */
// //______________________________________________________________________________
// //------------------------------------------------------------------------------
// auto CSindarinWiFi::setMode(wifi_mode_t mode) -> bool { return wiFiEsp.mode(mode); }
