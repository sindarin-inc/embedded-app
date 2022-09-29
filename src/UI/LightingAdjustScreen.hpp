#pragma once

#include "Storage/PreferencesStore.hpp"
#include "UI/Screen.hpp"

class LightingAdjustScreen : public Screen {
public:
    LightingAdjustScreen(PreferencesStore &prefs) : Screen(), prefs_(prefs) {}

    void processCommand(RemoteCommand command) override;
    void render(DisplaySystem &display) override;

    bool firstLoad = true;

protected:
    // Easter eggs are cool.
    bool lightUpMyLife_ = false;
    PreferencesStore &prefs_;
};
