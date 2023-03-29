#ifndef BATTERY_SCREEN_HPP
#define BATTERY_SCREEN_HPP

#include "Misc/Power.hpp"
#include "Renderers/Renderer.hpp"
#include "Storage/PreferencesStore.hpp"
#include "UI/Screen.hpp"

class BatteryScreen : public Screen {
public:
    BatteryScreen(DisplaySystem &display, PreferencesStore &prefs, Power &power)
        : display_(display), prefs_(prefs), power_(power){};

    void render(DisplaySystem &display) override;
    void processCommand(RemoteCommand command) override;

protected:
    DisplaySystem &display_;
    PreferencesStore &prefs_;
    Power &power_;
};

#endif