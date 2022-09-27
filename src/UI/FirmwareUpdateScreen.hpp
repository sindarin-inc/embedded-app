#ifndef FIRMWARE_UPDATE_SCREEN_HPP
#define FIRMWARE_UPDATE_SCREEN_HPP

#include "Misc/Power.hpp"
#include "Renderers/Renderer.hpp"
#include "Storage/PreferencesStore.hpp"
#include "UI/Screen.hpp"
#include "Communications/WiFi.hpp"
#include "Communications/FirmwareDownload.hpp"

/*
#define NUM_DEMO_SCREENS 5
#define DEMO_SCREEN_1 "Hello!"
#define DEMO_SCREEN_2 "Let's read together"
#define DEMO_SCREEN_3 "Press the right button to go to the next page"
#define DEMO_SCREEN_4                                                                              \
    "Now it seems like you are ready to read some real text. From the main menu, press the left "  \
    "or right buttons to cycle through the readables. Then press down when you want to read "      \
    "something."
#define DEMO_SCREEN_5 "Ready? Press up to go to the main menu."
*/

class FirmwareUpdateScreen : public Screen {
public:
    FirmwareUpdateScreen(DisplaySystem &display, PreferencesStore &prefs, Power &power);

    void render(DisplaySystem &display) override;
    void processCommand(RemoteCommand command) override;

protected:
    DisplaySystem &display_;
    Renderer renderer_;
    PreferencesStore &prefs_;
    Power &power_;
    FirmwareDownload firmwareDownload_;

    unsigned int currentScreen_ = 0;
};

#endif