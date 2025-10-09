#ifndef BATTERY_SCREEN_HPP
#define BATTERY_SCREEN_HPP

#include <Renderer/Renderer.hpp>

#include "Misc/Battery.hpp"
#include "Storage/PreferencesStore.hpp"
#include "UI/Screen.hpp"

class BatteryScreen : public Screen {
public:
    BatteryScreen(DisplaySystem &display, PreferencesStore &prefs, Battery &battery);

    void render(DisplaySystem &display) override;
    void processCommand(RemoteCommand command) override;

protected:
    DisplaySystem &display_;
    PreferencesStore &prefs_;
    Battery &battery_;
    uint16_t leftTemp_{0};
    uint16_t rightTemp_{0};
};

#endif