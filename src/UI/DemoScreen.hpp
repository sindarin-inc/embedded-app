#ifndef DEMO_SCREEN_HPP
#define DEMO_SCREEN_HPP

#include "Misc/Battery.hpp"
#include "Renderers/Renderer.hpp"
#include "Sensors/LightingController.hpp"
#include "Storage/PreferencesStore.hpp"
#include "UI/Screen.hpp"

#define NUM_DEMO_SCREENS 36
#define NUM_DEMO_MESSAGES 41
#define DEMO_SCREEN_1 "Focus left eye"
#define DEMO_SCREEN_2 "Focus right eye"
#define DEMO_SCREEN_3 "When focused, press any button"
#define DEMO_SCREEN_4 "Now put down the PageTurner and read!"
#define DEMO_SCREEN_5 "Five thousand years ago"
#define DEMO_SCREEN_6 "these words would have"
#define DEMO_SCREEN_6a "been carved in stone."
#define DEMO_SCREEN_7 "A millennium or so later"
#define DEMO_SCREEN_8 "you'd be unraveling a"
#define DEMO_SCREEN_8a "scroll of yellowed papyrus."
#define DEMO_SCREEN_9 "Jump forward a bit from there:"
#define DEMO_SCREEN_10 "paper!"
#define DEMO_SCREEN_11 "handwritten words"
#define DEMO_SCREEN_11a "then typset ones"
#define DEMO_SCREEN_11b "then...pixels."
#define DEMO_SCREEN_12 "The way we read is always changing"
#define DEMO_SCREEN_13 "Each leap forward solving"
#define DEMO_SCREEN_13a "different problems and"
#define DEMO_SCREEN_13b "sparking new possibilities."
#define DEMO_SCREEN_14 "But one thing hasn't changed"
#define DEMO_SCREEN_14a "across all these centuries:"
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
    std::array<std::string, NUM_DEMO_MESSAGES> messages_ = {
        DEMO_SCREEN_1,   DEMO_SCREEN_2,   DEMO_SCREEN_3,   DEMO_SCREEN_4,   DEMO_SCREEN_5,
        DEMO_SCREEN_6,   DEMO_SCREEN_6a,  DEMO_SCREEN_7,   DEMO_SCREEN_8,   DEMO_SCREEN_8a,
        DEMO_SCREEN_9,   DEMO_SCREEN_10,  DEMO_SCREEN_11,  DEMO_SCREEN_11a, DEMO_SCREEN_11b,
        DEMO_SCREEN_12,  DEMO_SCREEN_13,  DEMO_SCREEN_13a, DEMO_SCREEN_13b, DEMO_SCREEN_14,
        DEMO_SCREEN_14a, DEMO_SCREEN_14b, DEMO_SCREEN_15,  DEMO_SCREEN_16,  DEMO_SCREEN_16a,
        DEMO_SCREEN_16b, DEMO_SCREEN_16c, DEMO_SCREEN_16d, DEMO_SCREEN_16e, DEMO_SCREEN_17,
        DEMO_SCREEN_17a, DEMO_SCREEN_17b, DEMO_SCREEN_18,  DEMO_SCREEN_18a, DEMO_SCREEN_18b,
        DEMO_SCREEN_18c, DEMO_SCREEN_19,  DEMO_SCREEN_19a, DEMO_SCREEN_20,  DEMO_SCREEN_20a,
        DEMO_SCREEN_20b};
    DisplaySystem &display_;
    Renderer renderer_;
    PreferencesStore &prefs_;
    LightingController &lightingController_;

    unsigned int currentScreen_ = 0;
};

#endif