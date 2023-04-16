#pragma once

#include "Misc/SpiramAllocator.hpp"
#include "Models/Json.hpp"
#include "Models/JsonSerializable.hpp"
#include "Storage/PreferencesStore.hpp"

#define MAX_WIFI_SETTINGS 4
 
class WiFiSetting : public JsonSerializable {
public:
    WiFiSetting() = default;
    ~WiFiSetting() override = default;

    std::string ssid_;
    std::string password_;

    bool fromJson(const JsonObject &j) {
        return false;
    }
};

auto convertToJson(const WiFiSetting &src, JsonVariant dst) -> bool;
void convertFromJson(JsonVariantConst src, WiFiSetting &dst);

class WiFiSettings : JsonSerializable {
public:
    WiFiSettings() {}
    ~WiFiSettings() {}

    WiFiSetting settings[MAX_WIFI_SETTINGS];
    int numSettings_ = 0;

    int count() const {
        return numSettings_;
    }

    WiFiSetting& top() {
        return settings[0];
    }

    void rotate() {
        WiFiSetting temp = top();
        for (int i = 0; i < numSettings_-1; i++) {
            settings[i] = settings[i+1];
        }

        settings[numSettings_-1] = temp;
    }

    void save() const;

    void load();
    
    bool fromJson(const JsonObject &j) {
        JsonArray jsonSettings = j["settings"];
        log_i("in fromJson %d", jsonSettings.size());
        numSettings_ = jsonSettings.size();

        for (int i = 0; i < numSettings_; i++) {
            settings[i] = jsonSettings[i];
        }

        return false;
    }
};

auto convertToJson(const WiFiSettings &src, JsonVariant dst) -> bool;
//void convertFromJson(JsonVariantConst src, WiFiSettings &dst);