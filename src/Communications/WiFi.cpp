#if !DISABLE_WIFI

#include "Communications/WiFi.hpp"

#include <WiFi.h>
#include <esp_wifi.h>

void ConnectWiFiIfCredentialsExist() {
    // If credentials are available, go ahead and attempt WiFi connection
    if (!WiFi.enableSTA(true)) {
        log_e("STA enable failed!");
    }
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

#endif
