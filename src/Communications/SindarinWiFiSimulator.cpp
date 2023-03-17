#if SIMULATOR

#include <string>

#include "Misc/Platform.hpp"
#include "Communications/SindarinWiFi.hpp"
#include "config.h"
#include "sindarin-debug.h"

// Note(john): We might consider mocking this at the esp_* level like we've done with other esp
// components. But that's a little more complexity than I want to bite off right now. This is
// easier.

SindarinWiFi sindarinWiFi;

auto SindarinWiFi::begin() -> esp_err_t { return ESP_OK; }

auto SindarinWiFi::end() -> esp_err_t { return ESP_OK; }

auto SindarinWiFi::getWifiConfig(wifi_config_t *conf) -> esp_err_t { return ESP_OK; }

auto SindarinWiFi::hasWiFiCredentials(wifi_config_t *conf) -> bool { return true; }

auto SindarinWiFi::hasWiFiCredentials() -> bool { return true; }

auto SindarinWiFi::scanNetworks(
    const std::function<void(wifi_ap_record_t *apRecords, int recordCount)> &completeCB)
    -> esp_err_t {
    assert(false && "Not implemented");
    return ESP_OK;
}

auto SindarinWiFi::connectWiFi(const std::string &ssid, const std::string &password) -> esp_err_t {
    log_i("Pretending to connect WiFi (wiFiPower_=%d, ssid=%s, password=%s)", wiFiPower_,
          ssid.c_str(), password.c_str());
    delay(1000);
    return ESP_OK;
}

auto SindarinWiFi::connectWiFiWithTimeout(int timeoutMs) -> bool {
    log_i("Pretending to connect WiFi (wiFiPower_=%d, timeoutMs=%dms)", wiFiPower_, timeoutMs);
    delay(1000);
    return true;
}

void SindarinWiFi::disconnectWiFi() {
    log_i("Pretending to disconnect WiFi");
    connected_ = false;
}

auto SindarinWiFi::connected() -> bool { return connected_; }

auto SindarinWiFi::getLocalIp() -> esp_ip4_addr_t { return esp_ip4_addr{.addr = 0}; }

auto SindarinWiFi::getLocalIpStr() -> std::string { return "0.0.0.0"; }

#endif
