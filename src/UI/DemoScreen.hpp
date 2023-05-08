#ifndef DEMO_SCREEN_HPP
#define DEMO_SCREEN_HPP

#include "Misc/Battery.hpp"
#include "Renderers/Renderer.hpp"
#include "Sensors/LightingController.hpp"
#include "Storage/PreferencesStore.hpp"
#include "UI/Screen.hpp"

#define NUM_DEMO_SCREENS 44

#define SKIP_MESSAGE "Press \ue000 to skip"
#define DEMO_SCREEN_1a "Twist lens in front of LEFT eye"
#define DEMO_SCREEN_1b "like a binocular. When text is"
#define DEMO_SCREEN_1c "crisp, press \ue000 on your remote."
#define DEMO_SCREEN_2a "Twist lens in front of RIGHT eye"
#define DEMO_SCREEN_3a "Optionally, turn the knob on TOP "
#define DEMO_SCREEN_3b "to adjust lens spacing. When "
#define DEMO_SCREEN_3c "text is comfortable, press \ue000"
#define DEMO_SCREEN_4 "Now put down the Sol Remote and enjoy!"
#define DEMO_SCREEN_5a "The way we read"
#define DEMO_SCREEN_5b "is always changing."
#define DEMO_SCREEN_6a "But across the centuries"
#define DEMO_SCREEN_6b "one thing hasn't."
#define DEMO_SCREEN_7a "What reading can do"
#define DEMO_SCREEN_7b "for our minds:"
#define DEMO_SCREEN_8a "Whisking us away"
#define DEMO_SCREEN_8b "into other realms."
#define DEMO_SCREEN_9a "Sparking new"
#define DEMO_SCREEN_9b "thoughts and ideas."
#define DEMO_SCREEN_10a "In this era of"
#define DEMO_SCREEN_10b "constant beeps,"
#define DEMO_SCREEN_10c "alerts, and updates..."
#define DEMO_SCREEN_11a "...your Sol Reader"
#define DEMO_SCREEN_11b "provides a quiet"
#define DEMO_SCREEN_11c "respite from it all."
#define DEMO_SCREEN_12a "Simply lean back,"
#define DEMO_SCREEN_12b "open your eyes, and"
#define DEMO_SCREEN_12c "let words light the way."

//
// This class handles the Onboarding mode (or first time on), so Demo mode
// isn't quite the right name, but probably close enough?
//

class DemoScreen : public Screen {
public:
    DemoScreen(DisplaySystem &display, PreferencesStore &prefs,
               LightingController &lightingController);

    void render(DisplaySystem &display) override;
    void processCommand(RemoteCommand command) override;

protected:
    void drawSkipMessage(DisplaySystem &display, bool inverted = false);
    void drawDoubleLine(DisplaySystem &display, std::string one, std::string two);
    void drawTripleLine(DisplaySystem &display, std::string one, std::string two,
                        std::string three);

    DisplaySystem &display_;
    Renderer renderer_;
    PreferencesStore &prefs_;
    LightingController &lightingController_;

    bool shownOne_ = false;
    bool shownTwo_ = false;
    unsigned int currentScreen_ = 0;
    unsigned int currentSubScreen_ = 0;
};

#endif