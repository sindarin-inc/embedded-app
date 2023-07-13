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
    void loop(DisplaySystem &display, RemoteCommand command) override;
    void handleBrowse(FileSystem &fileSystem, MenuScreen *fsMenu, const std::string &path = "/");

protected:
    PreferencesStore &prefs_;
    PowerManager &power_;
    DisplaySystem *display_ = nullptr;
    Telemetry &telemetry_;
};
