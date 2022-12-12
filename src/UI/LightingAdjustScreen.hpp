#pragma once

#include "Sensors/LightingController.hpp"
#include "Storage/PreferencesStore.hpp"
#include "UI/Screen.hpp"

class LightingAdjustScreen : public Screen {
public:
    LightingAdjustScreen(PreferencesStore &prefs, LightingController &lightingController)
        : Screen(), prefs_(prefs), lightingController_(lightingController) {}

    void processCommand(RemoteCommand command) override;
    void render(DisplaySystem &display) override;

    bool firstLoad = true;

protected:
    // Easter eggs are cool.
    bool lightUpMyLife_ = false;
    PreferencesStore &prefs_;
    LightingController &lightingController_;
};
