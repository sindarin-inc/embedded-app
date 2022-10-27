#if SIMULATOR

#include <string>

#include "WiFi.hpp"
#include "sindarin-debug.h"

auto GetWiFiCredentials(wifi_config_t *conf) -> bool {
    strncpy((char *)conf->sta.ssid, "ssid", sizeof(conf->sta.ssid));
    strncpy((char *)conf->sta.password, "password", sizeof(conf->sta.password));
    return true;
}

auto HasWiFiCredentials() -> bool { return true; }

auto ConnectWiFiIfCredentialsExist() -> bool {
    log_i("Pretending to connect WiFi");
    return true;
}

void DisconnectWifi() { log_i("Pretending to disconnect WiFi"); }

auto NetworkConnected() -> bool { return true; }

auto ConnectWifi(int timeoutMs) -> bool {
    // Pretend to connect to WiFi
    delay(1000);
    return true;
}

#endif
