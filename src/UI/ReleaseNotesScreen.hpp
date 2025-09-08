#pragma once

#include "Misc/Battery.hpp"
#include "Misc/PowerManager.hpp"
#include "Models/Firmware.hpp"
#include "Renderers/Renderer.hpp"
#include "Storage/LocalStore.hpp"
#include "Storage/PreferencesStore.hpp"
#include "UI/LineMenuScreen.hpp"

class ReleaseNotesScreen : public Screen {
public:
    ReleaseNotesScreen(DisplaySystem &display, PreferencesStore &prefs, PowerManager &power,
                       LocalStore &localStore, const Firmware &firmware);

protected:
    void render(DisplaySystem &display) override;
    void processCommand(RemoteCommand command) override;

    DisplaySystem &display_;
    Renderer renderer_;
    PreferencesStore &prefs_;
    PowerManager &power_;
    LocalStore &localStore_;
    Firmware firmware_;
    LineMenuScreen menu_;

    std::shared_ptr<Place> place_, newPlace_;

    auto atEnd() -> bool { return renderer_.atEndOfFile(); };

    void updateError(const std::string &logMessage);
    void runUpdate();

    bool inMenu_ = false;
};
