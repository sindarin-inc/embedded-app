#ifndef LINE_SPACING_SCREEN_HPP
#define LINE_SPACING_SCREEN_HPP

#include "Misc/Power.hpp"
#include "Renderers/Renderer.hpp"
#include "Storage/PreferencesStore.hpp"
#include "UI/Screen.hpp"

// Eventually we'll want to refactor this with the OffsetAdjustScreen
// and ResizeBoxScreen since so much of the functionality is the same
// This is just to test that this concept works ok.
class LineSpacingScreen : public Screen {
public:
    LineSpacingScreen(DisplaySystem &display, PreferencesStore &prefs);

protected:
    void render(DisplaySystem &display) override;
    void processCommand(RemoteCommand command) override;

    DisplaySystem &display_;
    Renderer renderer_;
    PreferencesStore &prefs_;

    int lineSpacing_;
};

#endif