#pragma once

#include <vector>

#include "Displays/DisplaySystem.hpp"
#include "Inputs/RemoteInput.hpp"
#include "Misc/Battery.hpp"
#include "Misc/PowerManager.hpp"
#include "Models/Readable.hpp"
#include "Sensors/LightingController.hpp"
#include "Storage/FileSystem.hpp"
#include "Storage/LocalStore.hpp"
#include "Storage/PreferencesStore.hpp"
#include "UI/ContainerScreen.hpp"

class RootUI : public ContainerScreen {
public:
    RootUI(PreferencesStore &prefs, PowerManager &power) noexcept : prefs_(prefs), power_(power) {}
    RootUI(RootUI const &) = delete;

    void setup(DisplaySystem *display, LocalStore &localStore, FileSystem &fileSystem,
               LightingController &lightingController);
    void loop(DisplaySystem &display, RemoteCommand command) override;

protected:
    PreferencesStore &prefs_;
    PowerManager &power_;
    DisplaySystem *display_ = nullptr;
};
