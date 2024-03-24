#pragma once

#include "Misc/Battery.hpp"
#include "Misc/Telemetry.hpp"
#include "Renderers/Renderer.hpp"
#include "Storage/PreferencesStore.hpp"
#include "UI/Screen.hpp"

class FirstHookTestScreen : public Screen {
public:
    FirstHookTestScreen(DisplaySystem &display, PreferencesStore &prefs, Telemetry &telemetry);

    void render(DisplaySystem &display) override;
    void processCommand(RemoteCommand command) override;

protected:
    DisplaySystem &display_;
    Renderer renderer_;
    PreferencesStore &prefs_;
    Telemetry &telemetry_;
};
