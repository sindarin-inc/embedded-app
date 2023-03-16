#include "Communications/SindarinWiFi.hpp" // Class Header

#include <string>

#include "config.h"
#include "sindarin-debug.h"

CSindarinWiFi sindarinWiFi;

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

    began_ = true;
    return ESP_OK;
}

auto CSindarinWiFi::getWiFiCredentials(wifi_config_t *conf) -> bool {
    esp_err_t err = begin();
    if (err != ESP_OK) {
        log_e("begin failed with error %s", esp_err_to_name(err));
        return false;
    }

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

auto CSindarinWiFi::hasWiFiCredentials() -> bool {
    wifi_config_t conf;
    return getWiFiCredentials(&conf);
}

auto CSindarinWiFi::scanNetworks(
    const std::function<void(wifi_ap_record_t *apRecords, int recordCount)> &completeCB)
    -> esp_err_t {

    esp_err_t err = begin();
    if (err != ESP_OK) {
        log_e("begin failed with error %s", esp_err_to_name(err));
        return err;
    }

    err = esp_wifi_start();
    if (err != ESP_OK) {
        log_e("esp_wifi_start failed with error %s", esp_err_to_name(err));
        return err;
    }
    SHOW_HEAP();

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

    err = esp_wifi_stop();
    if (err != ESP_OK) {
        log_e("esp_wifi_stop failed with error %s", esp_err_to_name(err));
        return err;
    }
    SHOW_HEAP();

    return ESP_OK;
}

auto CSindarinWiFi::connectWiFi(const std::string &ssid, const std::string &password) -> esp_err_t {
    // We have to have wifi sleep enabled otherwise Bluetooth can't work
    esp_err_t err = esp_wifi_set_ps(WIFI_PS_MIN_MODEM);
    if (err != ESP_OK) {
        log_e("esp_wifi_set_ps failed with error %s", esp_err_to_name(err));
        return err;
    }

    // If credentials are available, go ahead and attempt WiFi connection
    // wifi_config_t conf;
    // getWiFiCredentials(&conf);
    // log_i("Found WiFi credentials for %s. Attempting connection", (const char *)conf.sta.ssid);
    // TODO: Hostname
    // wiFiEsp.setHostname(HOSTNAME);

    err = begin();
    if (err != ESP_OK) {
        log_e("begin failed with error %s", esp_err_to_name(err));
        return err;
    }

    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;
    err = esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wiFiEventHandler, this,
                                              &instance_any_id);
    if (err != ESP_OK) {
        log_e("esp_event_handler_instance_register failed with error %s", esp_err_to_name(err));
        return err;
    }

    err = esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &ipEventHandler, this,
                                              &instance_got_ip);
    if (err != ESP_OK) {
        log_e("esp_event_handler_instance_register failed with error %s", esp_err_to_name(err));
        return err;
    }

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

    err = esp_wifi_set_mode(WIFI_MODE_STA);
    if (err != ESP_OK) {
        log_e("esp_wifi_set_mode failed with error %s", esp_err_to_name(err));
        return err;
    }
    err = esp_wifi_set_config(WIFI_IF_STA, &wifi_config);
    if (err != ESP_OK) {
        log_e("esp_wifi_set_config failed with error %s", esp_err_to_name(err));
        return err;
    }
    err = esp_wifi_start();
    if (err != ESP_OK) {
        log_e("esp_wifi_start failed with error %s", esp_err_to_name(err));
        return err;
    }

    int8_t power = 0;
    esp_wifi_get_max_tx_power(&power);
    log_d("WiFi TX Power was %d", power);
    esp_wifi_set_max_tx_power(wiFiPower_);
    esp_wifi_get_max_tx_power(&power);
    log_d("WiFi TX Power is now %d", power);

    return ESP_OK;
}

auto CSindarinWiFi::connectWiFiIfCredentialsExist() -> bool {
    if (!hasWiFiCredentials()) {
        log_d("No WiFi credentials found");
        return false;
    }
    connectWiFi();
    return true;
}

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

void CSindarinWiFi::disconnectWiFi() {
    log_i("Disconnecting WiFi");
    autoReconnect_ = false;
    bool disconnected = esp_wifi_disconnect();
    if (!disconnected) {
        log_i("could not disconnect wifi");
    }

    esp_err_t err = esp_wifi_set_mode(WIFI_MODE_NULL);
    if (err != ESP_OK) {
        log_i("error setting wifi mode in disconnectWifi(): %d", err);
    }
}

auto CSindarinWiFi::connected() -> bool { return connected_; }

void CSindarinWiFi::wiFiEventHandler(void *arg, esp_event_base_t eventBase, int32_t eventId,
                                     void *eventData) {
    // CSindarinWiFi *tthis = (CSindarinWiFi *)arg;
    switch (eventId) {
    case WIFI_EVENT_STA_START:
        log_d("WIFI_EVENT_STA_START");
        esp_wifi_connect();
        break;
    case WIFI_EVENT_STA_DISCONNECTED:
        log_i("WIFI_EVENT_STA_DISCONNECTED");
        break;
    default:
        log_d("WIFI_EVENT: %d", eventId);
    }
}

auto CSindarinWiFi::getLocalIp() -> wifiesp_ip4address { 
    if (getMode() == WIFI_MODE_NULL) {
        return IPADDRESS_NONE;
    }
    esp_netif_ip_info_t ip;
    if (esp_netif_get_ip_info(_sta_netif, &ip) != ESP_OK) {
        log_i("Netif Get IP Failed");
        return IPADDRESS_NONE;
    }
    return (wifiesp_ip4address)ip.ip;
}

auto CSindarinWiFi::getLocalIpStr() -> std::string {
    esp_ip4_addr_t ip = (esp_ip4_addr_t)getLocalIp();
    char ipstr[16];
    sprintf(ipstr, IPSTR, IP2STR(&ip));
    return ipstr;
}

auto CSindarinWiFi::getMode() -> wifi_mode_t { 
    wifi_mode_t mode;
    if (esp_wifi_get_mode(&mode) != ESP_OK) {
        log_i("error getMode(): WiFi not started");
        return WIFI_MODE_NULL;
    }
}

void CSindarinWiFi::ipEventHandler(void *arg, esp_event_base_t eventBase, int32_t eventId,
                                   void *eventData) {
    CSindarinWiFi *tthis = (CSindarinWiFi *)arg;
    switch (eventId) {
    case IP_EVENT_STA_GOT_IP: {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)eventData;
        log_i("IP_EVENT_STA_GOT_IP: " IPSTR, IP2STR(&event->ip_info.ip));
        tthis->connected_ = true;
        break;
    }
    default:
        log_d("IP_EVENT: %d", eventId);
    }
}
