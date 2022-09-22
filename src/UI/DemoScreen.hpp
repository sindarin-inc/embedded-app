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
#define DEMO_SCREEN_5 "Let's read!"

class DemoScreen : public Screen {
public:
    DemoScreen(DisplaySystem &display, PreferencesStore &prefs, Power &power);

    void render(DisplaySystem &display) override;
    void processCommand(RemoteCommand command) override;

protected:
    std::array<std::string, NUM_DEMO_SCREENS> messages_ = {
        DEMO_SCREEN_1, DEMO_SCREEN_2, DEMO_SCREEN_3, DEMO_SCREEN_4, DEMO_SCREEN_5};
    DisplaySystem &display_;
    Renderer renderer_;
    PreferencesStore &prefs_;
    Power &power_;

    unsigned int currentScreen_ = 0;
};

#endif