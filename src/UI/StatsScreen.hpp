#ifndef STATS_SCREEN_HPP
#define STATS_SCREEN_HPP

#include "Misc/Power.hpp"
#include "Renderers/Renderer.hpp"
#include "Storage/PreferencesStore.hpp"
#include "UI/Screen.hpp"

class StatsScreen : public Screen {
public:
    StatsScreen(DisplaySystem &display, PreferencesStore &prefs);

    void render(DisplaySystem &display) override;
    void processCommand(RemoteCommand command) override;

protected:
    DisplaySystem &display_;
    PreferencesStore &prefs_;
};

#endif