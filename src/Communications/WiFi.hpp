#pragma once

#include <esp_wifi.h>

auto GetWiFiCredentials(wifi_config_t *conf) -> bool;

auto HasWiFiCredentials() -> bool;

auto ConnectWiFiIfCredentialsExist() -> bool;

void DisconnectWifi();

auto NetworkConnected() -> bool;

enum class WifiConnectState { NO_CREDENTIALS, UNABLE_TO_CONNECT, CONNECTED };

auto ConnectWifi(int timeoutMs = 20000) -> bool;
