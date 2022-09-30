#ifndef DEMO_SCREEN_HPP
#define DEMO_SCREEN_HPP

#include "Misc/Power.hpp"
#include "Renderers/Renderer.hpp"
#include "Storage/PreferencesStore.hpp"
#include "UI/Screen.hpp"

#define NUM_DEMO_SCREENS 5
#define DEMO_SCREEN_1 "Hello!"
#define DEMO_SCREEN_2 "Let's read together"
#define DEMO_SCREEN_3 "Focus left eye"
#define DEMO_SCREEN_4 "Focus right eye"
#define DEMO_SCREEN_5 "Sol Manifesto"
#define DEMO_SCREEN_6                                                                              \
    "Five thousand years ago, these words\n             would have been carved in stone."
#define DEMO_SCREEN_7                                                                              \
    "A couple thousand later, you'd be\n           unraveling a scroll of yellowed papyrus."
#define DEMO_SCREEN_8 "Jump forward a bit from there -- paper!"
#define DEMO_SCREEN_9 "Handwritten words. Then typeset ones.\n                         Then pixels."
#define DEMO_SCREEN_10 "The way we read is always changing."
#define DEMO_SCREEN_11                                                                             \
    "Each leap forward solving different\n              problems and sparking new possibilities."
#define DEMO_SCREEN_12 "But one thing hasn't changed\n          across all these centuries:"
#define DEMO_SCREEN_13 "What reading does for our minds."
#define DEMO_SCREEN_14 "Whisking us away into other realms."
#define DEMO_SCREEN_15 "Making us laugh or cry\n          or pause to ponder."
#define DEMO_SCREEN_16 "Illuminating new thoughts,\n              concepts, and ideas."
#define DEMO_SCREEN_17                                                                             \
    "In this era of constant beeps, \n            alerts and updates,\n           what will the "  \
    "next evolution of reading "                                                                   \
    "look like?"
#define DEMO_SCREEN_18                                                                             \
    "Simply lean back, open your eyes\n                 and let words light the way."

//
// This class handles the Onboarding mode (or first time on), so Demo mode
// isn't quite the right name, but probably close enough?
//

class DemoScreen : public Screen {
public:
    DemoScreen(DisplaySystem &display, PreferencesStore &prefs, Power &power);

    void render(DisplaySystem &display) override;
    void processCommand(RemoteCommand command) override;

protected:
    std::array<std::string, NUM_DEMO_SCREENS> messages_ = {
        DEMO_SCREEN_1,  DEMO_SCREEN_2,  DEMO_SCREEN_3,  DEMO_SCREEN_4,  DEMO_SCREEN_5,/*
        DEMO_SCREEN_6,  DEMO_SCREEN_7,  DEMO_SCREEN_8,  DEMO_SCREEN_9,  DEMO_SCREEN_10,
        DEMO_SCREEN_11, DEMO_SCREEN_12, DEMO_SCREEN_13, DEMO_SCREEN_14, DEMO_SCREEN_15,
        DEMO_SCREEN_16, DEMO_SCREEN_17, DEMO_SCREEN_18*/};
    DisplaySystem &display_;
    Renderer renderer_;
    PreferencesStore &prefs_;
    Power &power_;

    unsigned int currentScreen_ = 0;
};

#endif