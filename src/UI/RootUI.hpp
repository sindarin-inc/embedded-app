#pragma once

#include <vector>

#include "Displays/DisplaySystem.hpp"
#include "Inputs/RemoteInput.hpp"
#include "Misc/Power.hpp"
#include "Models/Readable.hpp"
#include "Storage/FileSystem.hpp"
#include "Storage/LocalStore.hpp"
#include "Storage/PreferencesStore.hpp"
#include "UI/ContainerScreen.hpp"

class RootUI : public ContainerScreen {
public:
    RootUI(PreferencesStore &prefs, Power &power) noexcept : prefs_(prefs), power_(power) {}

    void setup(DisplaySystem *display, LocalStore &localStore, FileSystem &fileSystem);
    void loop(DisplaySystem &display, RemoteCommand command) override;

protected:
    PreferencesStore &prefs_;
    Power &power_;
    DisplaySystem *display_ = nullptr;
};
