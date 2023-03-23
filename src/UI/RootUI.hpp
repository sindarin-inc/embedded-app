#pragma once

#include <vector>

#include "Displays/DisplaySystem.hpp"
#include "Inputs/RemoteInput.hpp"
#include "Misc/Battery.hpp"
#include "Models/Readable.hpp"
#include "Sensors/LightingController.hpp"
#include "Storage/FileSystem.hpp"
#include "Storage/LocalStore.hpp"
#include "Storage/PreferencesStore.hpp"
#include "UI/ContainerScreen.hpp"

class RootUI : public ContainerScreen {
public:
    RootUI(PreferencesStore &prefs, Battery &battery) noexcept : prefs_(prefs), battery_(battery) {}
    RootUI(RootUI const &) = delete;

    void setup(DisplaySystem *display, LocalStore &localStore, FileSystem &fileSystem,
               LightingController &lightingController);
    void loop(DisplaySystem &display, RemoteCommand command) override;

protected:
    PreferencesStore &prefs_;
    Battery &battery_;
    DisplaySystem *display_ = nullptr;
};
