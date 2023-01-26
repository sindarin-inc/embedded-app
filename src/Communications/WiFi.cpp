#include "Communications/WiFi.hpp"

#include <cstring>

#include "config.h"
#include "sindarin-debug.h"

// TODO: We probably have enough state around here to merit a class. Maybe we do that when we
// move off of Arduino WiFi and onto esp-idf's C WiFi APIs.
wifi_power_t wiFiPower = WIFI_POWER_19_5dBm;

auto GetWiFiCredentials(wifi_config_t *conf) -> bool {
    // Needs to be called before we can get the WiFi config
    if (!WiFi.enableSTA(true)) {
        log_e("STA enable failed!");
        return false;
    }

    if (esp_wifi_get_config(WIFI_IF_STA, conf) != ESP_OK) {
        log_e("Failed to get WiFi config");
        return false;
    }
    if (strlen((const char *)conf->sta.ssid) == 0) {
        return false;
    }
    if (strlen((const char *)conf->sta.password) == 0) {
        log_w("Zero length WiFi password");
    }
    return true;
}

auto HasWiFiCredentials() -> bool {
    wifi_config_t conf;
    return GetWiFiCredentials(&conf);
}

void ConnectWiFi(const std::string &ssid, const std::string &password) {
    // We have to have wifi sleep enabled otherwise Bluetooth can't work
    esp_wifi_set_ps(WIFI_PS_MIN_MODEM);

    // I'm not sure, but it might be that WiFi sleep before connecting to an AP could
    // be problematic.
    // if (!WiFi.setSleep(true)) {
    //     log_e("Failed to enable WiFi sleep");
    // }

    // If credentials are available, go ahead and attempt WiFi connection
    wifi_config_t conf;
    GetWiFiCredentials(&conf);
    log_i("Found WiFi credentials for %s. Attempting connection", (const char *)conf.sta.ssid);
    WiFi.setHostname(HOSTNAME);
    if (ssid.empty()) {
        WiFi.begin();
    } else {
        WiFi.begin(ssid.c_str(), password.c_str());
    }
    log_d("WiFi TX Power was %d", WiFi.getTxPower());
    WiFi.setTxPower(wiFiPower);
    log_d("WiFi TX Power is now %d", WiFi.getTxPower());
    WiFi.setAutoReconnect(true);
}

auto ConnectWiFiIfCredentialsExist() -> bool {
    if (!HasWiFiCredentials()) {
        log_d("No WiFi credentials found");
        return false;
    }
    ConnectWiFi();
    return true;
}

void DisconnectWifi() {
    log_i("Disconnecting WiFi");
    WiFi.setAutoReconnect(false);
    WiFi.disconnect();
    WiFi.mode(WIFI_OFF);
}

auto NetworkConnected() -> bool { return WiFi.isConnected(); }

auto ConnectWifi(int timeoutMs) -> bool {
    auto startTime = millis();
    if (!NetworkConnected()) {
        ConnectWiFiIfCredentialsExist();
        // TODO: Would be nice to make this not a polling loop
        while (!NetworkConnected() && (millis() - startTime) < timeoutMs) {
            delay(200);
        }
        if (!NetworkConnected()) {
            DisconnectWifi();
            return false;
        }
    }

    return true;
}

void SetWiFiPower(wifi_power_t power) { wiFiPower = power; }

auto GetWiFiPower() -> wifi_power_t { return wiFiPower; }
