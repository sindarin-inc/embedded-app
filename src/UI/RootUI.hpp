#pragma once

#include <vector>

#include "Contents/Readable.hpp"
#include "Displays/DisplaySystem.hpp"
#include "Inputs/RemoteInput.hpp"
#include "Misc/Power.hpp"
#include "Storage/PreferencesStore.hpp"
#include "UI/ContainerScreen.hpp"

class RootUI : public ContainerScreen {
public:
    RootUI(PreferencesStore &prefs, Power &power) : prefs_(prefs), power_(power) {}

    void setup(DisplaySystem *display, std::vector<Readable> &readables);
    void processCommand(RemoteCommand command) override;
    void loop(RemoteCommand command) override;

protected:
    PreferencesStore &prefs_;
    Power &power_;
    DisplaySystem *display_;

private:
    void restoreFactoryDefaults();
};
