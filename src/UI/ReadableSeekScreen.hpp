#pragma once

#include <utility>

#include "Models/Readable.hpp"
#include "Renderers/Renderer.hpp"
#include "Storage/PreferencesStore.hpp"
#include "UI/Screen.hpp"

class ReadableSeekScreen : public Screen {
public:
    ReadableSeekScreen(PreferencesStore &prefs, std::shared_ptr<Readable> readable,
                       Renderer &renderer)
        : Screen(), prefs_(prefs), readable_(std::move(readable)), renderer_(renderer) {}

    void processCommand(RemoteCommand command) override;
    void render(DisplaySystem &display) override;

    void setFirstLoad() { firstLoad_ = true; }

protected:
    PreferencesStore &prefs_;
    std::shared_ptr<Readable> readable_;
    Renderer &renderer_;
    bool firstLoad_ = true;

    // How much to seek in terms of denominator of the total number of pages.
    // Default is 20 (5%) -- 100 is 1%, 1000 is .1%, etc.
    unsigned int seekGranularity_ = 20;
};
