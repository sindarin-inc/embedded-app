#pragma once

#include "Models/Readable.hpp"
#include "Renderers/Renderer.hpp"
#include "Storage/PreferencesStore.hpp"
#include "UI/Screen.hpp"

class ReadableSeekScreen : public Screen {
public:
    ReadableSeekScreen(PreferencesStore &prefs, Readable &readable, Renderer &renderer)
        : Screen(), prefs_(prefs), readable_(readable), renderer_(renderer) {}

    void processCommand(RemoteCommand command) override;
    void render(DisplaySystem &display) override;
    bool firstLoad = true;

protected:
    PreferencesStore &prefs_;
    Readable &readable_;
    Renderer &renderer_;

    // How much to seek in terms of denominator of the total number of pages.
    // Default is 20 (5%) -- 100 is 1%, 1000 is .1%, etc.
    unsigned int seekGranularity_ = 20;
};
