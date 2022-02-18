#if !DISABLE_WIFI

#include "Communications/WiFi.hpp"

#include <WiFi.h>
#include <esp_wifi.h>

#include "config.h"

void ConnectWiFiIfCredentialsExist() {
    // If credentials are available, go ahead and attempt WiFi connection
    if (!WiFi.enableSTA(true)) {
        log_e("STA enable failed!");
    }
    // I'm not sure, but it might be that WiFi sleep before connecting to an AP could
    // be problematic.
    // if (!WiFi.setSleep(true)) {
    //     log_e("Failed to enable WiFi sleep");
    // }
    wifi_config_t conf;
    esp_wifi_get_config(WIFI_IF_STA, &conf);
    const char *ssid = (const char *)conf.sta.ssid;
    const char *password = (const char *)conf.sta.password;
    if (strlen(ssid) > 0 && strlen(password) > 0) {
        log_i("Found WiFi credentials for %s. Attempting connection", ssid);
        WiFi.setHostname(HOSTNAME);
        WiFi.begin();
        WiFi.setAutoReconnect(true);
    }
}

void DisconnectWifi() {
    log_i("Disconnecting WiFi");
    WiFi.setAutoReconnect(false);
    WiFi.disconnect();
    WiFi.mode(WIFI_OFF);
}

auto NetworkConnected() -> bool { return WiFi.isConnected(); }

#elif SIMULATOR

bool NetworkConnected() { return true; }

#else

bool NetworkConnected() { return false; }

#endif
