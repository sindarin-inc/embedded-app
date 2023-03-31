#ifndef VCOM_ADJUST_SCREEN_HPP
#define VCOM_ADJUST_SCREEN_HPP

#include "Misc/Battery.hpp"
#include "Renderers/Renderer.hpp"
#include "Storage/PreferencesStore.hpp"
#include "UI/Screen.hpp"

class VcomAdjustScreen : public Screen {
public:
    VcomAdjustScreen(DisplaySystem &display, PreferencesStore &prefs);

    void render(DisplaySystem &display) override;
    void processCommand(RemoteCommand command) override;

    int vCom;

protected:
    DisplaySystem &display_;
    Renderer renderer_;
    PreferencesStore &prefs_;
};

#endif