#ifndef CES_SCREEN_HPP
#define CES_SCREEN_HPP

#include "Misc/Battery.hpp"
#include "Renderers/Renderer.hpp"
#include "Sensors/LightingController.hpp"
#include "Storage/PreferencesStore.hpp"
#include "UI/Screen.hpp"

#define NUM_CES_SCREENS 3
#define CES_SCREEN_1 "Focus left eye"
#define CES_SCREEN_2 "Focus right eye"
#define CES_SCREEN_3 "When focused, press any button"

//
// CES Mode will Focus the Left eye, then the right, then prompt the user to
// press a button to continue. This will take them right to a readable.
//

class CesScreen : public Screen {
public:
    CesScreen(DisplaySystem &display, PreferencesStore &prefs,
              LightingController &lightingController);

    void render(DisplaySystem &display) override;
    void processCommand(RemoteCommand command) override;

protected:
    std::array<std::string, NUM_CES_SCREENS> messages_ = {CES_SCREEN_1, CES_SCREEN_2, CES_SCREEN_3};
    DisplaySystem &display_;
    Renderer renderer_;
    PreferencesStore &prefs_;
    LightingController &lightingController_;

    unsigned int currentScreen_ = 0;
};

#endif