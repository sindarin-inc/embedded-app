#pragma once

#include "Misc/Battery.hpp"
#include "Misc/PowerManager.hpp"
#include "Models/Firmware.hpp"
#include "Renderers/Renderer.hpp"
#include "Storage/LocalStore.hpp"
#include "Storage/PreferencesStore.hpp"
#include "UI/ContainerScreen.hpp"
#include "UI/LineMenuScreen.hpp"

class ReleaseNotesScreen : public ContainerScreen {
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

    bool firstLoad_ = true;
    uint32_t place_{0}, newPlace_{0};

    auto atEnd() -> bool { return newPlace_ == 0; };

    void updateError(const std::string &logMessage);
    void runUpdate();

    void setFirstLoad() { firstLoad_ = true; }
};
