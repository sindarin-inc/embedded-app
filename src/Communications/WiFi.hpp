#include <esp_wifi.h>

auto GetWiFiCredentials(wifi_config_t *conf) -> bool;

auto HasWiFiCredentials() -> bool;

auto ConnectWiFiIfCredentialsExist() -> bool;

void DisconnectWifi();

auto NetworkConnected() -> bool;
