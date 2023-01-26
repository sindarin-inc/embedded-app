#pragma once

#include <WiFi.h>
#include <esp_wifi.h>

auto GetWiFiCredentials(wifi_config_t *conf) -> bool;

auto HasWiFiCredentials() -> bool;

void ConnectWiFi(const std::string &ssid = "", const std::string &password = "");

auto ConnectWiFiIfCredentialsExist() -> bool;

void DisconnectWifi();

auto NetworkConnected() -> bool;

enum class WifiConnectState { NO_CREDENTIALS, UNABLE_TO_CONNECT, CONNECTED };

auto ConnectWifi(int timeoutMs = 20000) -> bool;

void SetWiFiPower(wifi_power_t power);

auto GetWiFiPower() -> wifi_power_t;
