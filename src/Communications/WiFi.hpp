#pragma once

#include <esp_err.h>
#include <esp_wifi.h>
#include <freertos/FreeRTOS.h>
#include <freertos/event_groups.h>
#include <functional>
#include <memory>
#include <string>

#include "Communications/WiFiSettings.hpp"
#include "Storage/PreferencesStore.hpp"

typedef enum {
    WIFI_POWER_19_5dBm = 78,   // 19.5dBm
    WIFI_POWER_19dBm = 76,     // 19dBm
    WIFI_POWER_18_5dBm = 74,   // 18.5dBm
    WIFI_POWER_17dBm = 68,     // 17dBm
    WIFI_POWER_15dBm = 60,     // 15dBm
    WIFI_POWER_14_5dBm = 58,   // 14.5dBm
    WIFI_POWER_13dBm = 52,     // 13dBm
    WIFI_POWER_11dBm = 44,     // 11dBm
    WIFI_POWER_8_5dBm = 34,    // 8.5dBm
    WIFI_POWER_7dBm = 28,      // 7dBm
    WIFI_POWER_5dBm = 20,      // 5dBm
    WIFI_POWER_2dBm = 8,       // 2dBm
    WIFI_POWER_MINUS_1dBm = -4 // -1dBm
} wifi_power_t;

typedef enum {
    UNINITIALIZED = 0,
    IDLE = 1 << 0,
    CONNECTING = 1 << 1,
    CONNECTED = 1 << 2,
    DISCONNECTED = 1 << 3
} WiFiState;

class WiFi {

public:
    static auto getInstance() -> WiFi &;

    WiFi() = default;
    WiFi(WiFi const &) = delete;

    // Credentials
    auto getWifiConfig(wifi_config_t *conf) -> esp_err_t;
    auto hasWiFiCredentials(wifi_config_t *conf) -> bool;
    auto hasWiFiCredentials() -> bool;

    void clearWiFiCredentials();

    // Scanning
    auto scanNetworks(const std::function<void(std::shared_ptr<wifi_ap_record_t[]> apRecords,
                                               int recordCount)> &channelScanCompleteCB)
        -> esp_err_t;

    // Connection
    auto connectWiFi(const std::string &ssid = "", const std::string &password = "") -> esp_err_t;
    auto connectWiFiFromSettings() -> bool;
    void disconnectWiFi();
    auto connected() -> bool;

    // Settings
    /*!
     * @brief Set the WiFi power level. Must be run before connecting.
     */
    void setWiFiPower(wifi_power_t power) { wiFiPower_ = power; }
    auto getWiFiPower() -> wifi_power_t { return wiFiPower_; }

    auto getLocalIp() -> esp_ip4_addr_t;
    auto getLocalIpStr() -> std::string;

    auto getFirstWiFiSSID() -> std::string {
        return hasWiFiCredentials() ? settings_.settings[0].ssid : "";
    }

protected:
    WiFiState wiFiState_ = UNINITIALIZED;
    wifi_power_t wiFiPower_ = WIFI_POWER_19_5dBm;
    esp_netif_t *staNetif_;
    esp_event_handler_instance_t wifiEventHandlerInstance_;
    esp_event_handler_instance_t ipEventHandlerInstance_;
    WiFiSettings settings_;
    EventGroupHandle_t wifiEventGroup_ = nullptr;

    // Lifecycle init/deinit
    auto begin() -> esp_err_t;
    auto end() -> esp_err_t;

    // Event handlers
    static void wiFiEventHandler(void *arg, esp_event_base_t eventBase, int32_t eventId,
                                 void *eventData);
    static void ipEventHandler(void *arg, esp_event_base_t eventBase, int32_t eventId,
                               void *eventData);

    auto connectWiFiSettingWithTimeout(int timeoutMs, const std::string &ssid,
                                       const std::string &password) -> bool;
    void migrateWiFiCredentials();
};
