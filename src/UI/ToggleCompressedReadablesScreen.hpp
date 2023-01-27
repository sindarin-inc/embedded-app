#pragma once

#include <string>
#include <utility>

#include "Storage/FileSystem.hpp"
#include "Storage/PreferencesStore.hpp"
#include "UI/Screen.hpp"

class ToggleCompressedReadablesScreen : public Screen {
public:
    ToggleCompressedReadablesScreen(DisplaySystem &display, PreferencesStore &prefs)
        : display_(display), prefs_(prefs){};

    void render(DisplaySystem &display) override;
    void processCommand(RemoteCommand command) override;
    void begin();

protected:
    DisplaySystem &display_;
    PreferencesStore &prefs_;
};
