#ifndef DEMO_SCREEN_HPP
#define DEMO_SCREEN_HPP

#include <Renderer/Renderer.hpp>

#include "Misc/Battery.hpp"
#include "Sensors/LightingController.hpp"
#include "Storage/PreferencesStore.hpp"
#include "UI/Screen.hpp"

//
// This class handles the Onboarding mode (or first time on), so Demo mode
// isn't quite the right name, but probably close enough?
//

class DemoScreen : public Screen {
public:
    DemoScreen(DisplaySystem &display, PreferencesStore &prefs, bool fullManifesto = false);

    void render(DisplaySystem &display) override;
    void processCommand(RemoteCommand command) override;

protected:
    void drawSkipMessage(DisplaySystem &display, bool inverted = false);
    void drawDoubleLine(DisplaySystem &display, std::string one, std::string two);
    void drawTripleLine(DisplaySystem &display, std::string one, std::string two, std::string three,
                        bool invert = false);
    // void drawSerial(DisplaySystem &display);

    void finishDemo();

    DisplaySystem &display_;
    Renderer renderer_;
    PreferencesStore &prefs_;

    bool originalInversion_ = false;

    bool dropVcom_ = false;

    unsigned int currentScreen_ = 0;

    unsigned int nextInterval_ = 0;

    bool awaitingButton_ = false;

    bool fullManifesto_ = false;

    DisplaySystem::UpdateType nextUpdateType_ = DisplaySystem::UpdateType::FULL;

    std::vector<std::function<void(DisplaySystem &)>> lambdas_;
};

#endif