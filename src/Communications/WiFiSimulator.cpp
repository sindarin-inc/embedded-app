#if SIMULATOR

#include <string>

#include "WiFi.hpp"
#include "sindarin-debug.h"

wifi_power_t WiFiPower = WIFI_POWER_19_5dBm;

auto GetWiFiCredentials(wifi_config_t *conf) -> bool {
    strncpy((char *)conf->sta.ssid, "ssid", sizeof(conf->sta.ssid));
    strncpy((char *)conf->sta.password, "password", sizeof(conf->sta.password));
    return true;
}

auto HasWiFiCredentials() -> bool { return true; }

auto ConnectWiFiIfCredentialsExist() -> bool {
    log_i("Pretending to connect WiFi (power %d)", WiFiPower);
    delay(1000);
    return true;
}

void DisconnectWifi() { log_i("Pretending to disconnect WiFi"); }

auto NetworkConnected() -> bool { return true; }

auto ConnectWifi(int timeoutMs) -> bool {
    log_i("Pretending to connect WiFi (power %d)", WiFiPower);
    delay(1000);
    return true;
}

void SetWiFiPower(wifi_power_t power) { WiFiPower = power; }

auto GetWiFiPower() -> wifi_power_t { return WiFiPower; }

#endif
