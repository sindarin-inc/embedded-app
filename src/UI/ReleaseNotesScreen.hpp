#pragma once

#include "Misc/Battery.hpp"
#include "Models/Firmware.hpp"
#include "Renderers/Renderer.hpp"
#include "Storage/PreferencesStore.hpp"
#include "UI/Screen.hpp"

class ReleaseNotesScreen : public Screen {
public:
    ReleaseNotesScreen(DisplaySystem &display, PreferencesStore &prefs, Firmware &firmware);

protected:
    void render(DisplaySystem &display) override;
    void processCommand(RemoteCommand command) override;

    DisplaySystem &display_;
    Renderer renderer_;
    PreferencesStore &prefs_;
    Firmware &firmware_;
};
