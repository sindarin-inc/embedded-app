#include "WiFiSettings.hpp"

#include "Storage/PreferencesStore.hpp"

extern PreferencesStore prefs;

// NOLINTNEXTLINE(readability-identifier-naming) - ArduinoJson requires this name
auto convertToJson(const WiFiSetting &src, JsonVariant dst) -> bool {
    dst["ssid"] = src.ssid;
    dst["password"] = src.password;
    return true;
}

// NOLINTNEXTLINE(readability-identifier-naming) - ArduinoJson requires this name
void convertFromJson(JsonVariantConst src, WiFiSetting &dst) {
    dst.ssid = src["ssid"].as<std::string>();
    dst.password = src["password"].as<std::string>();
}

// NOLINTNEXTLINE(readability-identifier-naming) - ArduinoJson requires this name
auto convertToJson(const WiFiSettings &src, JsonVariant dst) -> bool {
    auto settingsJson = dst.createNestedArray("settings");
    int numSettings = src.settings.size();
    for (int i = 0; i < numSettings; i++) {
        settingsJson.add(src.settings[i]);
    }
    return true;
}

void WiFiSettings::load() {
    if (loaded_) {
        return;
    }
    loaded_ = true;
    std::string settingsString = prefs.wifiSettingsJson.get().value_or("");
    if (settingsString.empty()) {
        // no json, no settings to load
        return;
    }
    SpiRamJsonDocument doc(MAX_WIFI_SETTINGS_JSON_SIZE);
    deserializeJson(doc, settingsString);
    auto obj = doc.as<JsonObject>();
    fromJson(obj);
}

void WiFiSettings::save() const {
    auto doc = std::make_unique<SpiRamJsonDocument>(MAX_WIFI_SETTINGS_JSON_SIZE);
    JsonObject body = doc->to<JsonObject>();
    convertToJson(*this, body);
    std::string wifiString = DumpJson(body);
    prefs.wifiSettingsJson.set(wifiString);
}

void WiFiSettings::setActiveSetting(std::string ssid, std::string password) {
    load();

    // Erase the setting from the vector
    for (auto it = settings.begin(); it != settings.end();) {
        if (it->ssid == ssid && it->password == password) {
            // Erase returns the next iterator
            it = settings.erase(it);
        } else {
            it++;
        }
    }

    // Stick the setting in the front
    settings.emplace(settings.begin(), std::move(ssid), std::move(password));

    // Truncate the end if we have too many settings
    if (settings.size() > MAX_WIFI_SETTINGS) {
        settings.erase(settings.end() - 1);
    }
}

void WiFiSettings::clear() { settings.clear(); }
