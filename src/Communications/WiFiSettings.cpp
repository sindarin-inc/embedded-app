#include "WiFiSettings.hpp"

#include "Storage/PreferencesStore.hpp"

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
    log_i("wifi settings load: %s", settingsString.c_str());
    if (settingsString.size() == 0) {
        // no json, no settings to load
        return;
    }
    DynamicJsonDocument doc(2048);
    deserializeJson(doc, settingsString);
    JsonObject obj = doc.as<JsonObject>();
    fromJson(obj);
    valid_ = true;
}

void WiFiSettings::save() const {
    auto doc = std::make_unique<SpiRamJsonDocument>(2048);
    JsonObject body = doc->to<JsonObject>();
    convertToJson(*this, body);
    std::string wifiString = DumpJson(body);

    log_i("wifi settings: saving: %s***", wifiString.c_str());
    prefs.wifiSettingsJson.set(wifiString);
}

bool WiFiSettings::settingExists(const std::string &ssid, const std::string &password) {
    for (int i = 0; i < numSettings_; i++) {
        if (settings[i].ssid_ == ssid && settings[i].password_ == password) {
            return true;
        }
    }

    return false;
}

void WiFiSettings::addSetting(const std::string &ssid, const std::string &password) {
    // if this setting does not currently exist, add it to the top
    if (settingExists(ssid, password)) {
        log_i("addSetting: %s %s already exists", ssid.c_str(), password.c_str());
        return;
    }

    numSettings_++;
    if (numSettings_ >= MAX_WIFI_SETTINGS) {
        numSettings_ = MAX_WIFI_SETTINGS;
    }

    for (int i = numSettings_ - 1; i > 0; i--) {
        settings[i] = settings[i - 1];
    }

    settings[0].ssid_ = ssid;
    settings[0].password_ = password;
    log_i("added setting %s %s", ssid.c_str(), password.c_str());
}

void WiFiSettings::clear() { numSettings_ = 0; }