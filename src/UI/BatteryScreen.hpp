#ifndef BATTERY_SCREEN_HPP
#define BATTERY_SCREEN_HPP

#include "Misc/Battery.hpp"
#include "Renderers/Renderer.hpp"
#include "Storage/PreferencesStore.hpp"
#include "UI/Screen.hpp"

class BatteryScreen : public Screen {
public:
    BatteryScreen(DisplaySystem &display, PreferencesStore &prefs, Battery &battery)
        : display_(display), prefs_(prefs), battery_(battery){};

    void render(DisplaySystem &display) override;
    void processCommand(RemoteCommand command) override;

protected:
    DisplaySystem &display_;
    PreferencesStore &prefs_;
    Battery &battery_;
};

#endif