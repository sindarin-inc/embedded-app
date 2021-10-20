#pragma once
#include <NimBLEDevice.h>
#include <Preferences.h>

#include "Communications/BLEManager.hpp"
#include "Communications/PocketAPI.hpp"
#include "Storage/PreferencesStore.hpp"
#include "sindarin-debug.h"

class BLESettings {

public:
    BLESettings(PreferencesStore *prefs);
    void listen_for_settings();

protected:
    PreferencesStore *prefs;
    class CharacteristicCallbacks : public BLECharacteristicCallbacks {
    public:
        BLESettings *enclosed;

        CharacteristicCallbacks(BLESettings *enclosure) { enclosed = enclosure; }

        void onWrite(BLECharacteristic *characteristic) override {
            std::string value = characteristic->getValue();
            if (value.length() > 0) {
                // TODO: Handle other types of settings values
                enclosed->prefs->setPocketApiToken(value.c_str());
                log_i("Got Pocket Access Token");
            }
        }
    };
};
