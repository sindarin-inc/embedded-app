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

    std::string ssid_;
    std::string password_;

    bool fromJson(const JsonObject &j) { return false; }
};

// NOLINTNEXTLINE(readability-identifier-naming) - ArduinoJson requires this name
auto convertToJson(const WiFiSetting &src, JsonVariant dst) -> bool;

// NOLINTNEXTLINE(readability-identifier-naming) - ArduinoJson requires this name
void convertFromJson(JsonVariantConst src, WiFiSetting &dst);

class WiFiSettings : JsonSerializable {
public:
    WiFiSettings() {}
    ~WiFiSettings() {}

    WiFiSetting settings[MAX_WIFI_SETTINGS];
    int numSettings_ = 0;
    bool valid_ = false;

    int count() {
        load();
        return numSettings_;
    }
    WiFiSetting &top() {
        load();
        return settings[0];
    }
    void rotate() {
        load();
        WiFiSetting temp = top();
        for (int i = 0; i < numSettings_ - 1; i++) {
            settings[i] = settings[i + 1];
        }

        settings[numSettings_ - 1] = temp;
    }
    void save() const;
    void addSetting(const std::string &ssid, const std::string &password);
    bool fromJson(const JsonObject &j) {
        JsonArray jsonSettings = j["settings"];
        numSettings_ = jsonSettings.size();

        for (int i = 0; i < numSettings_; i++) {
            settings[i] = jsonSettings[i];
        }

        return true;
    }
    void clear();
    bool isValid() { return valid_; }
    void load();

private:
    bool loaded_ = false;

    bool settingExists(const std::string &ssid, const std::string &password);
};

// NOLINTNEXTLINE(readability-identifier-naming) - ArduinoJson requires this name
auto convertToJson(const WiFiSettings &src, JsonVariant dst) -> bool;