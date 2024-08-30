#include "Communications/WiFi.hpp"

#include <cstring>
#include <string>

#include "Misc/Defer.hpp"
#include "Misc/SpiramAllocator.hpp"
#include "config.h"
#include "sindarin-debug.h"

extern WiFi wiFi;

auto WiFi::getInstance() -> WiFi & { return wiFi; }

auto WiFi::begin() -> esp_err_t {
    if (wiFiState_ != UNINITIALIZED) {
        return ESP_OK;
    }

    HEAP_PROFILE_START(WiFiBegin);
    defer { HEAP_PROFILE_END(WiFiBegin); };

    // create event group if uninitialized
    if (wifiEventGroup_ == nullptr) {
        wifiEventGroup_ = xEventGroupCreate();
    }

    esp_err_t err = esp_netif_init(); // ~5k
    if (err != ESP_OK) {
        log_e("esp_netif_init failed with error %s", esp_err_to_name(err));
        return err;
    }

    err = esp_event_loop_create_default(); // 4k
    if (err != ESP_OK) {
        log_e("esp_event_loop_create_default failed with error %s", esp_err_to_name(err));
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

    staNetif_ = esp_netif_create_default_wifi_sta(); // 1k
    if (staNetif_ == nullptr) {
        log_e("esp_netif_create_default_wifi_sta returned NULL") return ESP_ERR_NO_MEM;
    }

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

    err = esp_wifi_set_mode(WIFI_MODE_STA); // 0k
    if (err != ESP_OK) {
        log_e("esp_wifi_set_mode failed with error %s", esp_err_to_name(err));
        return err;
    }

    setState(WiFiState::IDLE);

    return ESP_OK;
}

auto WiFi::end() -> esp_err_t {
    if (wiFiState_ == UNINITIALIZED) {
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

    vEventGroupDelete(wifiEventGroup_);
    wifiEventGroup_ = nullptr;

    setState(WiFiState::UNINITIALIZED);
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
    if (strlen(reinterpret_cast<const char *>(conf->sta.ssid)) == 0) {
        return false;
    }
    if (strlen(reinterpret_cast<const char *>(conf->sta.password)) == 0) {
        log_w("Zero length WiFi password (possibly ok for open networks)");
    }
    return true;
}

void WiFi::migrateWiFiCredentials() {
    wifi_config_t conf;
    esp_err_t err = getWifiConfig(&conf);
    if (err != ESP_OK) {
        log_w("error getting Wifi Config in migrateWiFiCredentials: %d", err);
        return;
    }

    if (hasWiFiCredentials(&conf)) {
        std::string ssid = reinterpret_cast<char *>(conf.sta.ssid);
        std::string password = reinterpret_cast<char *>(conf.sta.password);
        settings_.setActiveSetting(ssid, password);
    }
    settings_.save();
}

auto WiFi::hasWiFiCredentials() -> bool {
    if (settings_.needsMigration()) {
        migrateWiFiCredentials();
    }

    if (settings_.count() == 0) {
        return false;
    }

    return true;
}

void WiFi::clearWiFiCredentials() {
    log_i("clearing wifi credentials.");
    settings_.clear();
    settings_.save();
}

auto WiFi::scanNetworks(const std::function<void(std::shared_ptr<wifi_ap_record_t[]> apRecords,
                                                 int recordCount)> &channelScanCompleteCB)
    -> esp_err_t {
    esp_err_t err = begin();
    if (err != ESP_OK) {
        log_e("begin failed with error %s", esp_err_to_name(err));
        return err;
    }

    setState(WiFiState::SCANNING);

    err = esp_wifi_start();
    if (err != ESP_OK) {
        log_e("esp_wifi_start failed with error %s", esp_err_to_name(err));
        return err;
    }
    SHOW_HEAP();

    wifi_scan_config_t params;
    memset(&params, 0, sizeof(wifi_scan_config_t));
    params.scan_type = WIFI_SCAN_TYPE_ACTIVE;

    // check possible wifi channels
    for (int ch = 1; ch <= 11; ch++) {
        params.channel = ch;
        log_i("scanning wifi channel %d", ch);
        err = esp_wifi_scan_start(&params, true); // 3k PSRAM
        if (err != ESP_OK) {
            log_e("esp_wifi_scan_start failed with error %s", esp_err_to_name(err));
            continue;
        }
        SHOW_HEAP();

        uint16_t apNum = 0;
        err = esp_wifi_scan_get_ap_num(&apNum); // 0k
        if (err != ESP_OK) {
            log_e("esp_wifi_scan_get_ap_num failed with error %s", esp_err_to_name(err));
            continue;
        }
        SHOW_HEAP();

        if (apNum == 0) {
            log_i("Channel %d, no APs found", ch);
            continue;
        }

        auto apRecords = SpiRamArrayPtr<wifi_ap_record_t>(apNum);

        err = esp_wifi_scan_get_ap_records(&apNum, apRecords.get());
        if (err != ESP_OK) {
            log_e("esp_wifi_scan_get_ap_records failed with error %s", esp_err_to_name(err));
            continue;
        }
        log_i("Channel %d, Total APs scanned = %u", ch, apNum);

        channelScanCompleteCB(apRecords, apNum);
    }
    SHOW_HEAP();

    err = esp_wifi_stop();
    setState(WiFiState::IDLE);
    if (err != ESP_OK) {
        log_e("esp_wifi_stop failed with error %s", esp_err_to_name(err));
        return err;
    }
    SHOW_HEAP();

    return ESP_OK;
}

auto WiFi::connectWiFi(const std::string &ssid, const std::string &password) -> esp_err_t {
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

    // We have to have wifi sleep enabled otherwise Bluetooth can't work
    err = esp_wifi_set_ps(WIFI_PS_MIN_MODEM);
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

    wifi_config_t wifiConfig = {};

    if (ssid.length() <= 0) {
        log_i("No WiFi credentials provided, using stored credentials");
        getWifiConfig(&wifiConfig);
    } else {
        log_i("Using provided WiFi credentials: %s/%s", ssid.c_str(), password.c_str());
        wifiConfig.sta.sae_pwe_h2e = WPA3_SAE_PWE_BOTH;
        // We do not need to null terminate `wifi_config_t`'s `ssid` and `password` fields so we can
        // ignore warnings about potential null termination missing.
#pragma GCC diagnostic push
// NOLINTNEXTLINE(clang-diagnostic-unknown-warning-option)
#pragma GCC diagnostic ignored "-Wstringop-truncation"
        strncpy(reinterpret_cast<char *>(wifiConfig.sta.ssid), ssid.c_str(),
                sizeof(wifiConfig.sta.ssid));
        strncpy(reinterpret_cast<char *>(wifiConfig.sta.password), password.c_str(),
                sizeof(wifiConfig.sta.password));
#pragma GCC diagnostic pop
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

    setState(WiFiState::CONNECTING);

    return ESP_OK;
}

auto WiFi::connectWiFiFromSettings() -> bool {

    if (connected()) {
        return true;
    }

    esp_err_t err = begin();
    if (err != ESP_OK) {
        log_e("begin failed with error %s", esp_err_to_name(err));
        return err;
    }

    if (settings_.needsMigration()) {
        migrateWiFiCredentials();
    }

    if (settings_.count() == 0) {
        log_w("no wifi credentials found");
        return false;
    }

    // loop twice through all available wifi settings
    for (int attempt = 0; attempt < 2; attempt++) {
        int settingsToCheck = settings_.count();
        for (int i = 0; i < settingsToCheck; i++) {
            if (connectWiFiSettingWithTimeout(20000, settings_.settings[i].ssid,
                                              settings_.settings[i].password)) {
                return true;
            }
        }
    }

    // failed to connect
    log_w("failed to connect to wifi");
    disconnectWiFi();
    return false;
}

auto WiFi::connectWiFiSettingWithTimeout(int timeoutMs, const std::string &ssid,
                                         const std::string &password) -> bool {
    if (connectWiFi(ssid, password) != ESP_OK) {
        log_w("connectWiFi failed");
        return false;
    }

    // wait on wifi events
    EventBits_t bits = xEventGroupWaitBits(wifiEventGroup_, CONNECTED | DISCONNECTED, true, false,
                                           pdMS_TO_TICKS(timeoutMs));
    if ((bits & CONNECTED) == 0) {
        esp_err_t err = esp_wifi_disconnect();
        if (err != ESP_OK) {
            log_w("error in esp_wifi_disconnect: %d", err);
        }
        setState(WiFiState::IDLE);
        return false;
    }

    return true;
}

void WiFi::disconnectWiFi(int waitMs) {
    if (xSemaphoreTake(mutex_, portMAX_DELAY) != pdTRUE) {
        log_e("WiFi Mutex Failure");
    }
    defer { xSemaphoreGive(mutex_); };

    if (wiFiState_ == UNINITIALIZED) {
        return;
    }

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

    // Optionally, wait for WiFi to stop
    if (waitMs > 0) {
        xEventGroupWaitBits(wifiEventGroup_, STOPPED, true, false, pdMS_TO_TICKS(waitMs));
    }

    SHOW_HEAP();

    end();

    SHOW_HEAP();
}

auto WiFi::connected() -> bool { return wiFiState_ == CONNECTED; }

auto WiFi::getLocalIp() -> esp_ip4_addr_t {
    esp_netif_ip_info_t ip;
    esp_err_t err = esp_netif_get_ip_info(staNetif_, &ip);
    if (err != ESP_OK) {
        log_e("esp_netif_get_ip_info failed with error %s", esp_err_to_name(err));
        return esp_ip4_addr{.addr = 0};
    }
    return {ip.ip};
}

auto WiFi::getLocalIpStr() -> std::string {
    esp_ip4_addr_t ip = getLocalIp();
    char ipstr[16];
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-cstyle-cast): There's a cast in IP2STR
    auto size = snprintf(ipstr, sizeof(ipstr), IPSTR, IP2STR(&ip));
    if (size < 0) {
        log_e("snprintf failed");
        return "";
    }
    return ipstr;
}

void WiFi::wiFiEventHandler(void *arg, esp_event_base_t eventBase, int32_t eventId,
                            void *eventData) {
    WiFi *tthis = static_cast<WiFi *>(arg);
    switch (eventId) {
    case WIFI_EVENT_STA_START:
        log_d("WIFI_EVENT_STA_START");
        break;
    case WIFI_EVENT_STA_DISCONNECTED: {
        auto *disconnected = static_cast<wifi_event_sta_disconnected_t *>(eventData);
        // These reasons are used in Espressif's WiFi provisioning component to
        // signify un-retryable auth problem. See:
        // https://github.com/espressif/esp-idf/blob/cbce221e88d52665523093b2b6dd0ebe3f1243f1/components/wifi_provisioning/src/manager.c#L953
        switch (disconnected->reason) {
        case WIFI_REASON_AUTH_EXPIRE:
        case WIFI_REASON_4WAY_HANDSHAKE_TIMEOUT:
        case WIFI_REASON_AUTH_FAIL:
        case WIFI_REASON_HANDSHAKE_TIMEOUT:
        case WIFI_REASON_MIC_FAILURE:
            log_e("WiFi Auth Error (%d)", disconnected->reason);
            tthis->retryable = false;
            break;
        case WIFI_REASON_NO_AP_FOUND:
            log_e("No AP found");
            tthis->retryable = true;
            break;
        default:
            log_e("WiFi Disconnect Error (%d)", disconnected->reason);
            tthis->retryable = true;
            break;
        }

        tthis->telemetry_.logMetric(TelemetryField::METRIC_WIFI, "disconnect",
                                    {{"reason", disconnected->reason}});

        tthis->setState(WiFiState::DISCONNECTED);
        xEventGroupSetBits(tthis->wifiEventGroup_, DISCONNECTED);
        break;
    }
    case WIFI_EVENT_STA_STOP:
        log_d("WIFI_EVENT_STA_STOP");
        xEventGroupSetBits(tthis->wifiEventGroup_, STOPPED);
        break;
    default:
        log_d("WIFI_EVENT: %" PRIi32, eventId);
    }
}

void WiFi::ipEventHandler(void *arg, esp_event_base_t eventBase, int32_t eventId, void *eventData) {
    auto *tthis = static_cast<WiFi *>(arg);
    switch (eventId) {
    case IP_EVENT_STA_GOT_IP: {
        auto *event = static_cast<ip_event_got_ip_t *>(eventData);
        // NOLINTNEXTLINE(cppcoreguidelines-pro-type-cstyle-cast): There's a cast in IP2STR
        log_i("IP_EVENT_STA_GOT_IP: " IPSTR, IP2STR(&event->ip_info.ip));

        // successful connection
        // save current wifi settings
        log_i("successful connection, saving settings.");
        wifi_config_t conf;
        esp_err_t err = esp_wifi_get_config(WIFI_IF_STA, &conf);
        if (err != ESP_OK) {
            log_e("got ip: esp_wifi_get_config failed with error %s", esp_err_to_name(err));
            break;
        }

        std::string ssid = reinterpret_cast<char *>(conf.sta.ssid);
        std::string password = reinterpret_cast<char *>(conf.sta.password);

        log_d("Saving most-recent WiFi: %s", ssid.c_str());

        tthis->settings_.setActiveSetting(ssid, password);
        tthis->settings_.save();
        tthis->setState(WiFiState::CONNECTED);
        xEventGroupSetBits(tthis->wifiEventGroup_, CONNECTED);
        break;
    }
    default:
        log_d("IP_EVENT: %" PRIi32, eventId);
    }
}
