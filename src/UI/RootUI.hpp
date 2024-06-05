#pragma once

#include <vector>

#include "Displays/DisplaySystem.hpp"
#include "Inputs/RemoteInput.hpp"
#include "Misc/Battery.hpp"
#include "Misc/PowerManager.hpp"
#include "Misc/Telemetry.hpp"
#include "Models/Readable.hpp"
#include "Sensors/LightingController.hpp"
#include "Storage/FileSystem.hpp"
#include "Storage/LocalStore.hpp"
#include "Storage/PreferencesStore.hpp"
#include "UI/ContainerScreen.hpp"
#include "UI/MenuScreen.hpp"

class RootUI : public ContainerScreen {
public:
    RootUI(PreferencesStore &prefs, PowerManager &power, Telemetry &telemetry) noexcept
        : prefs_(prefs), power_(power), telemetry_(telemetry) {}
    RootUI(RootUI const &) = delete;

    void setup(DisplaySystem *display, LocalStore &localStore, FileSystem &fileSystem,
               LightingController &lightingController);
    void handleBrowse(FileSystem &fileSystem, MenuScreen *fsMenu, const std::string &path = "/");

protected:
    std::string itemErrorMsg_ =
        "INTERNAL ERROR!\nThe item you tried to access\ncannot be read or "
        "found.\nPlease contact support.\n\nPress\u00a0\u00a0\ue000\u00a0\u00a0to return";
    std::string settingsRestartMsg_ = "Settings change required restart!\nRestarting...";

    PreferencesStore &prefs_;
    PowerManager &power_;
    DisplaySystem *display_ = nullptr;
    Telemetry &telemetry_;
};
