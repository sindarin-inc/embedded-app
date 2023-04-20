#pragma once

#include "Misc/SpiramAllocator.hpp"
#include "Models/Json.hpp"
#include "Models/JsonSerializable.hpp"
#include "Storage/PreferencesStore.hpp"

#define MAX_WIFI_SETTINGS 4
#define MAX_WIFI_SETTINGS_JSON_SIZE 4096

class WiFiSetting : public JsonSerializable {
public:
    WiFiSetting() = default;
    ~WiFiSetting() override = default;

    std::string ssid;
    std::string password;

    auto fromJson(const JsonObject &j) -> bool override { return false; }
};

// NOLINTNEXTLINE(readability-identifier-naming) - ArduinoJson requires this name
auto convertToJson(const WiFiSetting &src, JsonVariant dst) -> bool;

// NOLINTNEXTLINE(readability-identifier-naming) - ArduinoJson requires this name
void convertFromJson(JsonVariantConst src, WiFiSetting &dst);

class WiFiSettings : JsonSerializable {
public:
    WiFiSettings() = default;
    ~WiFiSettings() override = default;

    SpiramVector<WiFiSetting> settings;

    auto count() -> int {
        load();
        return settings.size();
    }
    void setActiveSetting(const std::string &ssid, const std::string &password);
    void save() const;
    auto fromJson(const JsonObject &j) -> bool override {
        JsonArray jsonSettings = j["settings"];
        int numSettings = jsonSettings.size();
        settings.clear();
        for (int i = 0; i < numSettings; i++) {
            settings.push_back(jsonSettings[i]);
        }

        return true;
    }
    void clear();
    void load();
    auto needsMigration() -> bool;

private:
    bool loaded_ = false;
    void addSetting(const std::string &ssid, const std::string &password);
    auto settingExists(const std::string &ssid, const std::string &password) -> bool;
};

// NOLINTNEXTLINE(readability-identifier-naming) - ArduinoJson requires this name
auto convertToJson(const WiFiSettings &src, JsonVariant dst) -> bool;