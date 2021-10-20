#pragma once
#include <NimBLEDevice.h>
#include <Preferences.h>

#include "Communications/BLEManager.hpp"
#include "Communications/PocketAPI.hpp"
#include "Renderers/RawRenderer.hpp"
#include "Storage/PreferencesStore.hpp"
#include "sindarin-debug.h"

class BLEReadableServer {

public:
    BLEReadableServer(RawRenderer **renderer, int *update_box, OpticsSystem *my_optics,
                      DisplaySystem *my_display)
        : renderer(renderer), update_box(update_box), my_optics(my_optics),
          my_display(my_display){};
    void setup();

protected:
    RawRenderer **renderer;
    int *update_box;
    OpticsSystem *my_optics;
    DisplaySystem *my_display;

    class CharacteristicCallbacks : public BLECharacteristicCallbacks {
    public:
        BLEReadableServer *enclosed;
        CharacteristicCallbacks(BLEReadableServer *enclosure) : enclosed(enclosure) {}

        void onWrite(BLECharacteristic *characteristic) override {
            std::string value = characteristic->getValue();
            *(enclosed->renderer) =
                new RawRenderer(enclosed->my_optics, enclosed->my_display, value);
            SIZE_OF(**(enclosed->renderer));
            *(enclosed->update_box) = 1;
            if (value.length() > 0) {
                DEBUG_LN("*********");
                DEBUG_F("New value: %s \n", value.c_str());
                DEBUG_LN("*********");
            }
        }
    };
};
