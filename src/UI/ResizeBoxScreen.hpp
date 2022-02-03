#ifndef RESIZE_BOX_SCREEN_HPP
#define RESIZE_BOX_SCREEN_HPP

#include "Misc/Power.hpp"
#include "Renderers/StreamRenderer.hpp"
#include "Storage/PreferencesStore.hpp"
#include "UI/Screen.hpp"

// Eventually we'll want to refactor this with the OffsetAdjustScreen
// since so much of the functionality is the same
// This is just to test that this concept works ok.
class ResizeBoxScreen : public Screen {
public:
    ResizeBoxScreen(DisplaySystem &display, PreferencesStore &prefs, Power &power);

protected:
    void render(DisplaySystem &display) override;
    void processCommand(RemoteCommand command) override;

    float boxWidthOffset_;
    float boxHeightOffset_;

    DisplaySystem &display_;
    StreamRenderer renderer_;
    PreferencesStore &prefs_;
    Power &power_;
};

#endif