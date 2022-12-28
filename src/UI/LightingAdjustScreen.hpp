#pragma once

#include "Sensors/LightingController.hpp"
#include "Storage/PreferencesStore.hpp"
#include "UI/Screen.hpp"
#include "UI/Views/LightingAdjustView.hpp"

class LightingAdjustScreen : public Screen {
public:
    LightingAdjustScreen(DisplaySystem &display, PreferencesStore &prefs,
                         LightingController &lightingController);

    void processCommand(RemoteCommand command) override;
    void render(DisplaySystem &display) override;

    bool firstLoad = true;

protected:
    LightingAdjustView view_;
    PreferencesStore &prefs_;
    LightingController &lightingController_;
};
