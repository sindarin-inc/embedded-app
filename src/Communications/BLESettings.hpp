#pragma once
#include "Communications/BLEManager.hpp"
#include "sindarin-debug.h"
#include "Communications/PocketAPI.hpp"

#include <NimBLEDevice.h>
#include <Preferences.h>


class BLESettings {

  public:
    BLESettings(Preferences *prefs);
    void listen_for_settings();

  protected:
    Preferences *prefs;
    class CharacteristicCallbacks : public BLECharacteristicCallbacks
    {
      public: 

      BLESettings* enclosed;
      
      CharacteristicCallbacks(BLESettings* enclosure) { 
        enclosed = enclosure;
      }

      void onWrite(BLECharacteristic *characteristic)
      {
        std::string value = characteristic->getValue();
        if (value.length() > 0) {
          enclosed->prefs->putString(POCKET_API_TOKEN_PREFS_KEY, value.c_str());
          log_i("Got Pocket Access Token");
        }
      }
    };
};
