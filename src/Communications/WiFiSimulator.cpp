#if SIMULATOR

#include <string>

#include "Communications/WiFi.hpp"
#include "Misc/Platform.hpp"
#include "config.h"
#include "sindarin-debug.h"

// Note(john): We might consider mocking this at the esp_* level like we've done with other esp
// components. But that's a little more complexity than I want to bite off right now. This is
// easier.

extern WiFi wiFi;

auto WiFi::getInstance() -> WiFi & { return wiFi; }

auto WiFi::begin() -> esp_err_t { return ESP_OK; }

auto WiFi::end() -> esp_err_t { return ESP_OK; }

auto WiFi::getWifiConfig(wifi_config_t *conf) -> esp_err_t { return ESP_OK; }

auto WiFi::hasWiFiCredentials() -> bool { return true; }

auto WiFi::scanNetworks(const std::function<void(std::shared_ptr<wifi_ap_record_t[]> apRecords,
                                                 int recordCount)> &completeCB) -> esp_err_t {
    assert(false && "Not implemented");
    return ESP_OK;
}

auto WiFi::connectWiFi(const std::string &ssid, const std::string &password) -> esp_err_t {
    log_i("Pretending to connect WiFi (wiFiPower_=%d, ssid=%s, password=%s)", wiFiPower_,
          ssid.c_str(), password.c_str());
// This causes problems when testing Content Sync outside of the context of a FreeRTOS task
#if !TESTS
    Delay(1000);
#endif
    setState(WiFiState::CONNECTED);
    return ESP_OK;
}

auto WiFi::connectWiFiFromSettings() -> bool {
    log_i("Pretending to connect WiFi (wiFiPower_=%d)", wiFiPower_);
#if !TESTS
    Delay(1000);
#endif
    setState(WiFiState::CONNECTED);
    return true;
}

void WiFi::disconnectWiFi(int waitMs) {
    log_i("Pretending to disconnect WiFi for %d ms", waitMs);
    setState(WiFiState::UNINITIALIZED);
}

auto WiFi::connected() -> bool { return wiFiState_ == CONNECTED; }

auto WiFi::getLocalIp() -> esp_ip4_addr_t { return esp_ip4_addr{.addr = 0}; }

auto WiFi::getLocalIpStr() -> std::string { return "0.0.0.0"; }

void WiFi::clearWiFiCredentials() { log_i("Pretending to clear WiFi credentials"); }

#endif
