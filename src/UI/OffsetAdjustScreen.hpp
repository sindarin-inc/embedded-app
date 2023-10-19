#ifndef OFFSET_ADJUST_SCREEN_HPP
#define OFFSET_ADJUST_SCREEN_HPP

#include "Misc/Battery.hpp"
#include "Misc/Telemetry.hpp"
#include "Renderers/Renderer.hpp"
#include "Storage/PreferencesStore.hpp"
#include "UI/Screen.hpp"

class OffsetAdjustScreen : public Screen {
public:
    OffsetAdjustScreen(DisplaySystem &display, PreferencesStore &prefs, Telemetry &telemetry);

    void render(DisplaySystem &display) override;
    void processCommand(RemoteCommand command) override;

protected:
    DisplaySystem &display_;
    Renderer renderer_;
    PreferencesStore &prefs_;
    Telemetry &telemetry_;
};

#endif
