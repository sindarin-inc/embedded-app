#include "Communications/WiFi.hpp"

#include <cstring>
#include <string>

#include "config.h"
#include "sindarin-debug.h"

WiFi wiFi;

auto WiFi::getInstance() -> WiFi & { return wiFi; }

auto WiFi::begin() -> esp_err_t {
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
    staNetif_ = esp_netif_create_default_wifi_sta(); // 1k
    if (staNetif_ == nullptr) {
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

auto WiFi::end() -> esp_err_t {
    if (!began_) {
        return ESP_OK;
    }

    esp_err_t err = esp_wifi_deinit();
    if (err != ESP_OK) {
        log_e("esp_wifi_deinit failed with error %s", esp_err_to_name(err));
        return err;
    }

    esp_netif_destroy_default_wifi(staNetif_);

    err =
        esp_event_handler_instance_unregister(IP_EVENT, ESP_EVENT_ANY_ID, ipEventHandlerInstance_);
    if (err != ESP_OK) {
        log_e("esp_event_handler_instance_unregister failed with error %s", esp_err_to_name(err));
        return err;
    }

    err = esp_event_handler_instance_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID,
                                                wifiEventHandlerInstance_);
    if (err != ESP_OK) {
        log_e("esp_event_handler_instance_unregister failed with error %s", esp_err_to_name(err));
        return err;
    }

    err = esp_event_loop_delete_default();
    if (err != ESP_OK) {
        log_e("esp_event_loop_delete_default failed with error %s", esp_err_to_name(err));
        return err;
    }

    err = esp_netif_deinit();
    // This is not supported by LWIP so it is expected to fail with ESP_ERR_NOT_SUPPORTED.
    // But we keep it here for completeness since that's what IDF does.
    // See: https://github.com/espressif/esp-idf/issues/4628#issuecomment-593983526
    if (err != ESP_OK && err != ESP_ERR_NOT_SUPPORTED) {
        log_e("esp_netif_deinit failed with error %s", esp_err_to_name(err));
        return err;
    }

    began_ = false;
    return ESP_OK;
}

auto WiFi::getWifiConfig(wifi_config_t *conf) -> esp_err_t {
    esp_err_t err = begin();
    if (err != ESP_OK) {
        log_e("begin failed with error %s", esp_err_to_name(err));
        return err;
    }

    if (esp_wifi_get_config(WIFI_IF_STA, conf) != ESP_OK) {
        log_e("esp_wifi_get_config failed with error %s", esp_err_to_name(err));
        return err;
    }

    return ESP_OK;
}

auto WiFi::hasWiFiCredentials(wifi_config_t *conf) -> bool {
    if (strlen((const char *)conf->sta.ssid) == 0) {
        return false;
    }
    if (strlen((const char *)conf->sta.password) == 0) {
        log_w("Zero length WiFi password (possibly ok for open networks)");
    }
    return true;
}

auto WiFi::hasWiFiCredentials() -> bool {
    wifi_config_t conf;
    esp_err_t err = getWifiConfig(&conf);
    if (err != ESP_OK) {
        log_e("getWifiConfig failed with error %s", esp_err_to_name(err));
        return false;
    }
    return hasWiFiCredentials(&conf);
}

auto WiFi::scanNetworks(
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
    if (err != ESP_OK) {
        log_e("esp_wifi_scan_get_ap_records failed with error %s", esp_err_to_name(err));
        return err;
    }
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

auto WiFi::connectWiFi(const std::string &ssid, const std::string &password) -> esp_err_t {
    // We have to have wifi sleep enabled otherwise Bluetooth can't work
    esp_err_t err = esp_wifi_set_ps(WIFI_PS_MIN_MODEM);
    if (err != ESP_OK) {
        log_e("esp_wifi_set_ps failed with error %s", esp_err_to_name(err));
        return err;
    }

    esp_netif_set_hostname(staNetif_, HOSTNAME);

    err = begin();
    if (err != ESP_OK) {
        log_e("begin failed with error %s", esp_err_to_name(err));
        return err;
    }

    err = esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wiFiEventHandler, this,
                                              &wifiEventHandlerInstance_);
    if (err != ESP_OK) {
        log_e("esp_event_handler_instance_register failed with error %s", esp_err_to_name(err));
        return err;
    }

    err = esp_event_handler_instance_register(IP_EVENT, ESP_EVENT_ANY_ID, &ipEventHandler, this,
                                              &ipEventHandlerInstance_);
    if (err != ESP_OK) {
        log_e("esp_event_handler_instance_register failed with error %s", esp_err_to_name(err));
        return err;
    }

    wifi_config_t wifiConfig = {};

    if (ssid.length() <= 0 || password.length() <= 0) {
        log_i("No WiFi credentials provided, using stored credentials");
        getWifiConfig(&wifiConfig);
    } else {
        log_i("Using provided WiFi credentials: %s/%s", ssid.c_str(), password.c_str());
        wifiConfig.sta.sae_pwe_h2e = WPA3_SAE_PWE_BOTH;
        strncpy((char *)wifiConfig.sta.ssid, ssid.c_str(), sizeof(wifiConfig.sta.ssid));
        strncpy((char *)wifiConfig.sta.password, password.c_str(), sizeof(wifiConfig.sta.password));
    }

    err = esp_wifi_set_mode(WIFI_MODE_STA);
    if (err != ESP_OK) {
        log_e("esp_wifi_set_mode failed with error %s", esp_err_to_name(err));
        return err;
    }
    err = esp_wifi_set_config(WIFI_IF_STA, &wifiConfig);
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

    err = esp_wifi_connect();
    if (err != ESP_OK) {
        log_e("esp_wifi_connect failed with error %s", esp_err_to_name(err));
        return err;
    }

    return ESP_OK;
}

auto WiFi::connectWiFiWithTimeout(int timeoutMs) -> bool {
    auto startTime = xTaskGetTickCount();
    if (!connected()) {
        wifi_config_t wifiConfig = {};
        getWifiConfig(&wifiConfig);
        if (!hasWiFiCredentials()) {
            log_d("No WiFi credentials found");
            return false;
        }
        if (connectWiFi() != ESP_OK) {
            log_w("connectWiFi failed");
            return false;
        }
        // Wait for a connection
        // TODO: Would be good to make this not a polling loop so if there's an obvious failure we
        // could return sooner
        while (!connected() && (xTaskGetTickCount() - startTime) < timeoutMs) {
            vTaskDelay(200);
        }
        if (!connected()) {
            log_w("Timed out waiting for WiFi connection");
            disconnectWiFi();
            return false;
        }
    }

    return true;
}

void WiFi::disconnectWiFi() {
    SHOW_HEAP();
    log_i("Disconnecting WiFi");

    esp_err_t err = esp_wifi_disconnect();
    if (err != ESP_OK) {
        log_e("esp_wifi_disconnect failed with %s", esp_err_to_name(err));
    }
    err = esp_wifi_set_mode(WIFI_MODE_NULL);
    if (err != ESP_OK) {
        log_e("esp_wifi_set_mode failed with %s", esp_err_to_name(err));
    }
    err = esp_wifi_stop();
    if (err != ESP_OK) {
        log_e("esp_wifi_stop failed with %s", esp_err_to_name(err));
    }

    SHOW_HEAP();

    end();

    SHOW_HEAP();
}

auto WiFi::connected() -> bool { return connected_; }

auto WiFi::getLocalIp() -> esp_ip4_addr_t {
    esp_netif_ip_info_t ip;
    esp_err_t err = esp_netif_get_ip_info(staNetif_, &ip);
    if (err != ESP_OK) {
        log_e("esp_netif_get_ip_info failed with error %s", esp_err_to_name(err));
        return esp_ip4_addr{.addr = 0};
    }
    return (esp_ip4_addr_t)ip.ip;
}

auto WiFi::getLocalIpStr() -> std::string {
    esp_ip4_addr_t ip = getLocalIp();
    char ipstr[16];
    auto size = snprintf(ipstr, sizeof(ipstr), IPSTR, IP2STR(&ip));
    if (size < 0) {
        log_e("snprintf failed");
        return "";
    }
    return ipstr;
}

void WiFi::wiFiEventHandler(void *arg, esp_event_base_t eventBase, int32_t eventId,
                            void *eventData) {
    WiFi *tthis = (WiFi *)arg;
    switch (eventId) {
    case WIFI_EVENT_STA_START:
        log_d("WIFI_EVENT_STA_START");
        break;
    case WIFI_EVENT_STA_DISCONNECTED:
        log_i("WIFI_EVENT_STA_DISCONNECTED");
        tthis->connected_ = false;
        break;
    default:
        log_d("WIFI_EVENT: %" PRIi32, eventId);
    }
}

void WiFi::ipEventHandler(void *arg, esp_event_base_t eventBase, int32_t eventId, void *eventData) {
    auto *tthis = (WiFi *)arg;
    switch (eventId) {
    case IP_EVENT_STA_GOT_IP: {
        auto *event = (ip_event_got_ip_t *)eventData;
        log_i("IP_EVENT_STA_GOT_IP: " IPSTR, IP2STR(&event->ip_info.ip));
        tthis->connected_ = true;
        break;
    }
    default:
        log_d("IP_EVENT: %" PRIi32, eventId);
    }
}
