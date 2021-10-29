#pragma once

#include "Contents/Readable.hpp"
#include "Displays/DisplaySystem.hpp"
#include "Inputs/RemoteInput.hpp"
#include "Optics/OpticsSystem.hpp"
#include "Renderers/Renderer.hpp"
#include "Storage/PreferencesStore.hpp"
#include "UI/Screen.hpp"

class RootUI {
public:
    RootUI(PreferencesStore &prefs) : prefs(prefs) {}

    void setup(DisplaySystem *display, OpticsSystem *optics, std::vector<Readable> &readables);
    void loop(RemoteCommand remote_command);

protected:
    Renderer *renderer;
    PreferencesStore &prefs;

    Screen *current_screen;
    void set_current_screen(Screen &screen) {
        screen.needs_rerender = true;
        current_screen = &screen;
    }
};
