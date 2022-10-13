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

    bool seekAndYouWillFind_ = false;
};
