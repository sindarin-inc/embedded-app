#ifndef RESIZE_BOX_SCREEN_HPP
#define RESIZE_BOX_SCREEN_HPP

#include "Misc/Battery.hpp"
#include "Misc/Telemetry.hpp"
#include "Renderers/Renderer.hpp"
#include "Storage/PreferencesStore.hpp"
#include "UI/Screen.hpp"

// Eventually we'll want to refactor this with the OffsetAdjustScreen
// since so much of the functionality is the same
// This is just to test that this concept works ok.
class ResizeBoxScreen : public Screen {
public:
    ResizeBoxScreen(DisplaySystem &display, PreferencesStore &prefs, Telemetry &telemetry);

protected:
    void render(DisplaySystem &display) override;
    void processCommand(RemoteCommand command) override;

    DisplaySystem &display_;
    Renderer renderer_;
    PreferencesStore &prefs_;
    Telemetry &telemetry_;
};

#endif
