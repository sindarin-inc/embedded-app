#ifndef DEMO_SCREEN_HPP
#define DEMO_SCREEN_HPP

#include "Misc/Battery.hpp"
#include "Renderers/Renderer.hpp"
#include "Sensors/LightingController.hpp"
#include "Storage/PreferencesStore.hpp"
#include "UI/Screen.hpp"

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

    void bailAfterLensAdjust() { bailAfterLens_ = true; }

protected:
    void drawSkipMessage(DisplaySystem &display, bool inverted = false);
    void drawDoubleLine(DisplaySystem &display, std::string one, std::string two);
    void drawTripleLine(DisplaySystem &display, std::string one, std::string two, std::string three,
                        bool invert = false);

    void finishDemo();

    DisplaySystem &display_;
    Renderer renderer_;
    PreferencesStore &prefs_;
    LightingController &lightingController_;

    bool shownOne_ = false;
    bool shownTwo_ = false;
    bool bailAfterLens_ = false;

    bool dropVcom_ = false;
    bool skipNextRender_ = false;

    bool shownAppDownload_ = false;

    unsigned int currentScreen_ = 0;
    unsigned int currentSubScreen_ = 0;

    unsigned int nextInterval_ = 0;
    int nextVcomNudge_ = 0;

    DisplaySystem::UpdateType nextUpdateType_ = DisplaySystem::UpdateType::FULL;

    std::vector<std::function<void(DisplaySystem &)>> lambdas_;
};

#endif