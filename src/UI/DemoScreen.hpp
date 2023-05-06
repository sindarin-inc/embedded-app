#ifndef DEMO_SCREEN_HPP
#define DEMO_SCREEN_HPP

#include "Misc/Battery.hpp"
#include "Renderers/Renderer.hpp"
#include "Sensors/LightingController.hpp"
#include "Storage/PreferencesStore.hpp"
#include "UI/Screen.hpp"

#define NUM_DEMO_SCREENS 44
#define NUM_DEMO_MESSAGES 41

#define SKIP_MESSAGE "Press (SOL) to skip"
#define DEMO_SCREEN_1a "Twist lens in front of LEFT"
#define DEMO_SCREEN_1b "eye like a binocular. When"
#define DEMO_SCREEN_1c "text is crisp, press SOL"
#define DEMO_SCREEN_2a "Twist lens in front of RIGHT"
#define DEMO_SCREEN_3a "Optionally, turn the knob on TOP "
#define DEMO_SCREEN_3b "to adjust lens spacing. When "
#define DEMO_SCREEN_3c "text is comfortable, press SOL"
#define DEMO_SCREEN_4 "Now put down the Sol Remote and read!"
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
#define DEMO_SCREEN_11a "...what will the"
#define DEMO_SCREEN_11b "next evolution of"
#define DEMO_SCREEN_11c "reading look like?"
#define DEMO_SCREEN_12a "Simply lean back,"
#define DEMO_SCREEN_12b "open your eyes, and"
#define DEMO_SCREEN_12c "let words light the way."
#define DEMO_SCREEN_14b "What reading does for our minds."
#define DEMO_SCREEN_15 "Whisking us away into other realms."
#define DEMO_SCREEN_16 "Making us laugh"
#define DEMO_SCREEN_16a "or cry"
#define DEMO_SCREEN_16b "or"
#define DEMO_SCREEN_16c "pause"
#define DEMO_SCREEN_16d "to"
#define DEMO_SCREEN_16e "ponder."
#define DEMO_SCREEN_17 "Illuminating new"
#define DEMO_SCREEN_17a "thoughts, concepts, and"
#define DEMO_SCREEN_17b "ideas."
#define DEMO_SCREEN_18 "In this era of constant"
#define DEMO_SCREEN_18a "beeps"
#define DEMO_SCREEN_18b "alerts"
#define DEMO_SCREEN_18c "and updates"
#define DEMO_SCREEN_19 "what will the next evolution"
#define DEMO_SCREEN_19a "of reading look like?"
#define DEMO_SCREEN_20 "Simply lean back,"
#define DEMO_SCREEN_20a "open your eyes,"
#define DEMO_SCREEN_20b "and let words light the way."
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

    std::array<std::string, NUM_DEMO_MESSAGES> messages_ = {
        DEMO_SCREEN_1a,  DEMO_SCREEN_2a,  DEMO_SCREEN_3a,  DEMO_SCREEN_4,   DEMO_SCREEN_5a,
        DEMO_SCREEN_6a,  DEMO_SCREEN_6b,  DEMO_SCREEN_7a,  DEMO_SCREEN_8a,  DEMO_SCREEN_8b,
        DEMO_SCREEN_9a,  DEMO_SCREEN_10a, DEMO_SCREEN_10b, DEMO_SCREEN_10c, DEMO_SCREEN_10c,
        DEMO_SCREEN_11a, DEMO_SCREEN_11b, DEMO_SCREEN_11c, DEMO_SCREEN_12a, DEMO_SCREEN_12b,
        DEMO_SCREEN_12c, DEMO_SCREEN_14b, DEMO_SCREEN_15,  DEMO_SCREEN_16,  DEMO_SCREEN_16a,
        DEMO_SCREEN_16b, DEMO_SCREEN_16c, DEMO_SCREEN_16d, DEMO_SCREEN_16e, DEMO_SCREEN_17,
        DEMO_SCREEN_17a, DEMO_SCREEN_17b, DEMO_SCREEN_18,  DEMO_SCREEN_18a, DEMO_SCREEN_18b,
        DEMO_SCREEN_18c, DEMO_SCREEN_19,  DEMO_SCREEN_19a, DEMO_SCREEN_20,  DEMO_SCREEN_20a,
        DEMO_SCREEN_20b};
    DisplaySystem &display_;
    Renderer renderer_;
    PreferencesStore &prefs_;
    LightingController &lightingController_;

    unsigned int currentScreen_ = 0;
    unsigned int currentSubScreen_ = 0;
};

#endif