#pragma once

#include <NimBLEDevice.h>
#include <Preferences.h>

#include "BLEAbstractions.hpp"
#include "Communications/BLEManager.hpp"
#include "Communications/PocketAPI.hpp"
#include "Storage/PreferencesStore.hpp"
#include "sindarin-debug.h"

class BLESettings : BLECharacteristicProvider {

public:
    BLESettings(PreferencesStore *prefs);
    void configureCharacteristics(BLEService *service) override;

protected:
    PreferencesStore *prefs_;
    class CharacteristicCallbacks : public BLECharacteristicCallbacks {
    public:
        BLESettings *enclosed;

        CharacteristicCallbacks(BLESettings *enclosure) { enclosed = enclosure; }

        void onWrite(BLECharacteristic *characteristic) override {
            std::string value = characteristic->getValue();
            if (value.length() > 0) {
                // TODO: Handle other types of settings values
                enclosed->prefs_->pocketApiToken.set(String(value.c_str()));
                log_i("Got Pocket Access Token");
            }
        }
    };
};
