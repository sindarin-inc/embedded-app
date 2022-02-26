#ifndef OFFSET_ADJUST_SCREEN_HPP
#define OFFSET_ADJUST_SCREEN_HPP

#include "Misc/Power.hpp"
#include "Renderers/Renderer.hpp"
#include "Storage/PreferencesStore.hpp"
#include "UI/Screen.hpp"

class OffsetAdjustScreen : public Screen {
public:
    OffsetAdjustScreen(DisplaySystem &display, PreferencesStore &prefs, Power &power);

    void render(DisplaySystem &display) override;
    void processCommand(RemoteCommand command) override;

    int xOffset;
    int yOffset;

protected:
    DisplaySystem &display_;
    Renderer renderer_;
    PreferencesStore &prefs_;
    Power &power_;
};

#endif