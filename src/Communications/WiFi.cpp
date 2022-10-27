#include "Communications/WiFi.hpp"

#include <WiFi.h>
#include <cstring>

#include "config.h"
#include "sindarin-debug.h"

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

auto ConnectWiFiIfCredentialsExist() -> bool {
    // I'm not sure, but it might be that WiFi sleep before connecting to an AP could
    // be problematic.
    // if (!WiFi.setSleep(true)) {
    //     log_e("Failed to enable WiFi sleep");
    //     return false;
    // }

    // We have to have wifi sleep enabled otherwise Bluetooth can't work
    esp_wifi_set_ps(WIFI_PS_MIN_MODEM);

    wifi_config_t conf;
    if (!GetWiFiCredentials(&conf)) {
        log_d("No WiFi credentials found");
        return false;
    }

    // If credentials are available, go ahead and attempt WiFi connection
    log_i("Found WiFi credentials for %s. Attempting connection", (const char *)conf.sta.ssid);
    WiFi.setHostname(HOSTNAME);
    WiFi.begin();
    WiFi.setAutoReconnect(true);

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
