#pragma once

#include "Storage/PreferencesStore.hpp"
#include "UI/Screen.hpp"

class LightingAdjustScreen : public Screen {
public:
    LightingAdjustScreen(PreferencesStore &prefs) : prefs_(prefs), Screen() {}

    void processCommand(RemoteCommand command) override;
    void render(DisplaySystem &display) override;

    bool firstLoad = true;

protected:
    PreferencesStore &prefs_;
};
