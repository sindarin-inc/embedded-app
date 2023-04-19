#include "WiFiSettings.hpp"

#include "Storage/PreferencesStore.hpp"

extern PreferencesStore prefs;

// NOLINTNEXTLINE(readability-identifier-naming) - ArduinoJson requires this name
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
    if (settingsString.size() == 0) {
        // no json, no settings to load
        return;
    }
    SpiRamJsonDocument doc(MAX_WIFI_SETTINGS_JSON_SIZE);
    deserializeJson(doc, settingsString);
    JsonObject obj = doc.as<JsonObject>();
    fromJson(obj);
    valid_ = true;
}

void WiFiSettings::save() const {
    auto doc = std::make_unique<SpiRamJsonDocument>(MAX_WIFI_SETTINGS_JSON_SIZE);
    JsonObject body = doc->to<JsonObject>();
    convertToJson(*this, body);
    std::string wifiString = DumpJson(body);
    prefs.wifiSettingsJson.set(wifiString);
}

bool WiFiSettings::settingExists(const std::string &ssid, const std::string &password) {
    for (auto s : settings) {
        if (s.ssid_ == ssid && s.password_ == password) {
            return true;
        }
    }
    return false;
}

void WiFiSettings::setActiveSetting(const std::string &ssid, const std::string &password) {
    // find setting
    for (auto it = settings.begin(); it != settings.end(); it++) {
        if (it->ssid_ == ssid && it->password_ == password) {
            settings.erase(it);
            break;
        }
    }

    addSetting(ssid, password);
}

void WiFiSettings::addSetting(const std::string &ssid, const std::string &password) {
    load();
    // if this setting does not currently exist, add it to the top
    if (settingExists(ssid, password)) {
        log_d("addSetting: %s %s already exists", ssid.c_str(), password.c_str());
        return;
    }

    WiFiSetting s;
    s.ssid_ = ssid;
    s.password_ = password;

    settings.insert(settings.begin(), s);
    if (settings.size() > MAX_WIFI_SETTINGS) {
        settings.erase(settings.end() - 1);
    }
    log_d("added setting %s %s", ssid.c_str(), password.c_str());
}

void WiFiSettings::clear() { settings.clear(); }