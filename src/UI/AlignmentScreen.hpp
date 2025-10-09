#ifndef ALIGNMENT_SCREEN_HPP
#define ALIGNMENT_SCREEN_HPP

#include <Renderer/Renderer.hpp>

#include "Misc/Battery.hpp"
#include "Sensors/LightingController.hpp"
#include "Storage/PreferencesStore.hpp"
#include "UI/Screen.hpp"

class AlignmentScreen : public Screen {
public:
    AlignmentScreen(DisplaySystem &display, PreferencesStore &prefs,
                    LightingController &lightingController);

    void render(DisplaySystem &display) override;
    void processCommand(RemoteCommand command) override;

protected:
    DisplaySystem &display_;
    PreferencesStore &prefs_;
    LightingController &lightingController_;

    unsigned int currentScreen_ = 0;
};

#endif
