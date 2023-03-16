#pragma once

#include <esp_wifi.h>
#include <functional>
#include <memory>
#include <string>

#include "Communications/WiFi/WiFiESP.hpp"

class CSindarinWiFi {

public:
    /// Default Constructor
    CSindarinWiFi() = default;

    auto begin() -> esp_err_t;

    // Credentials
    auto getWiFiCredentials(wifi_config_t *conf) -> bool;
    auto hasWiFiCredentials() -> bool;

    // Scanning
    auto scanNetworks(
        const std::function<void(wifi_ap_record_t *apRecords, int recordCount)> &completeCB)
        -> esp_err_t;

    // Connection
    auto connectWiFi(const std::string &ssid = "", const std::string &password = "") -> esp_err_t;
    auto connectWiFiIfCredentialsExist() -> bool;
    auto connectWiFiWithTimeout(int timeoutMs = 20000) -> bool;
    void disconnectWiFi();
    auto connected() -> bool;

    // Settings
    void setWiFiPower(wifi_power_t power) { wiFiPower_ = power; }
    auto getWiFiPower() -> wifi_power_t { return wiFiPower_; }

protected:
    bool began_ = false;
    bool connected_ = false;
    wifi_power_t wiFiPower_ = WIFI_POWER_19_5dBm;
    bool autoReconnect_ = false;

    static void wiFiEventHandler(void *arg, esp_event_base_t eventBase, int32_t eventId,
                                 void *eventData);
    static void ipEventHandler(void *arg, esp_event_base_t eventBase, int32_t eventId,
                               void *eventData);
};

// Global Object
extern CSindarinWiFi sindarinWiFi;
