#include "Storage/PreferencesStore.hpp"
#include "WiFiSettings.hpp"

extern PreferencesStore prefs;

auto convertToJson(const WiFiSetting &src, JsonVariant dst) -> bool {
    dst["ssid"] = src.ssid_;
    dst["password"] = src.password_;
    return true;
}

// NOLINTNEXTLINE(readability-identifier-naming) - ArduinoJson requires this name
void convertFromJson(JsonVariantConst src, WiFiSetting &dst) {
    dst.ssid_ = src["ssid"].as<std::string>();
    dst.password_ = src["password"].as<std::string>();
}

auto convertToJson(const WiFiSettings &src, JsonVariant dst) -> bool {
    
    auto settingsJson = dst.createNestedArray("settings");
    int numSettings = src.count();
    for (int i = 0; i < numSettings; i++) {
        settingsJson.add(src.settings[i]);
    }
    return true;
}

void WiFiSettings::load() {
    std::string settingsString = prefs.wifiSettingsJson.get().value_or("");
    DynamicJsonDocument doc(2048);
    deserializeJson(doc, settingsString);
    JsonObject obj = doc.as<JsonObject>();
    fromJson(obj);
}

void WiFiSettings::save() const {
    auto doc = std::make_unique<SpiRamJsonDocument>(2048);
    JsonObject body = doc->to<JsonObject>();
    convertToJson(*this, body);
    std::string wifiString = DumpJson(body);
    prefs.wifiSettingsJson.set(wifiString);
}